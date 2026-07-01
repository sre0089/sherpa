"use strict";

const { EventEmitter } = require("node:events");

const MAXIMUM_MESSAGE_SIZE = 64 * 1024 * 1024;

function encodeMessage(message) {
  const payload = Buffer.from(JSON.stringify(message), "utf8");
  return Buffer.concat([
    Buffer.from(`Content-Length: ${payload.length}\r\n\r\n`, "ascii"),
    payload,
  ]);
}

class MessageReader {
  constructor(onMessage) {
    this.onMessage = onMessage;
    this.buffer = Buffer.alloc(0);
  }

  push(chunk) {
    this.buffer = Buffer.concat([this.buffer, Buffer.from(chunk)]);
    while (true) {
      const headerEnd = this.buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0) {
        return;
      }

      const header = this.buffer.subarray(0, headerEnd).toString("ascii");
      const match = /^Content-Length:\s*(\d+)$/im.exec(header);
      if (!match) {
        throw new Error("missing Content-Length header");
      }
      const contentLength = Number.parseInt(match[1], 10);
      if (!Number.isSafeInteger(contentLength) || contentLength > MAXIMUM_MESSAGE_SIZE) {
        throw new Error("invalid Content-Length header");
      }

      const payloadStart = headerEnd + 4;
      const messageEnd = payloadStart + contentLength;
      if (this.buffer.length < messageEnd) {
        return;
      }
      const payload = this.buffer.subarray(payloadStart, messageEnd).toString("utf8");
      this.buffer = this.buffer.subarray(messageEnd);
      this.onMessage(JSON.parse(payload));
    }
  }
}

class JsonRpcError extends Error {
  constructor(error) {
    super(error.message);
    this.name = "JsonRpcError";
    this.code = error.code;
    this.data = error.data;
  }
}

class JsonRpcClient extends EventEmitter {
  constructor(childProcess) {
    super();
    this.childProcess = childProcess;
    this.nextId = 1;
    this.pending = new Map();
    this.disposed = false;
    this.reader = new MessageReader((message) => this.handleMessage(message));

    childProcess.stdout.on("data", (chunk) => {
      try {
        this.reader.push(chunk);
      } catch (error) {
        this.failAll(error);
      }
    });
    childProcess.stderr.on("data", (chunk) => this.emit("stderr", chunk.toString("utf8")));
    childProcess.on("error", (error) => this.failAll(error));
    childProcess.on("exit", (code, signal) => {
      this.failAll(new Error(`sherpa-server exited (code=${code}, signal=${signal})`));
      this.emit("exit", code, signal);
    });
  }

  sendRequest(method, params = {}) {
    if (this.disposed) {
      throw new Error("JSON-RPC client is disposed");
    }
    const id = this.nextId++;
    let rejectRequest;
    const promise = new Promise((resolve, reject) => {
      rejectRequest = reject;
      this.pending.set(id, { resolve, reject });
    });
    try {
      this.write({
        jsonrpc: "2.0",
        id,
        method,
        params,
      });
    } catch (error) {
      this.pending.delete(id);
      rejectRequest(error);
    }
    return {
      id,
      promise,
      cancel: () => this.cancel(id),
    };
  }

  notify(method, params = {}) {
    if (this.disposed) {
      throw new Error("JSON-RPC client is disposed");
    }
    this.write({
      jsonrpc: "2.0",
      method,
      params,
    });
  }

  cancel(id) {
    if (this.pending.has(id)) {
      this.notify("$/cancelRequest", { id });
    }
  }

  dispose() {
    this.disposed = true;
    this.failAll(new Error("JSON-RPC client disposed"));
  }

  write(message) {
    if (!this.childProcess.stdin.writable) {
      throw new Error("sherpa-server stdin is not writable");
    }
    this.childProcess.stdin.write(encodeMessage(message));
  }

  handleMessage(message) {
    if (!message || message.jsonrpc !== "2.0" || !Object.hasOwn(message, "id")) {
      return;
    }
    const pending = this.pending.get(message.id);
    if (!pending) {
      return;
    }
    this.pending.delete(message.id);
    if (message.error) {
      pending.reject(new JsonRpcError(message.error));
    } else {
      pending.resolve(message.result);
    }
  }

  failAll(error) {
    for (const pending of this.pending.values()) {
      pending.reject(error);
    }
    this.pending.clear();
  }
}

module.exports = {
  JsonRpcClient,
  JsonRpcError,
  MessageReader,
  encodeMessage,
};
