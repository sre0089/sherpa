"use strict";

const assert = require("node:assert/strict");
const { EventEmitter } = require("node:events");
const { PassThrough } = require("node:stream");
const test = require("node:test");

const {
  JsonRpcClient,
  JsonRpcError,
  MessageReader,
  encodeMessage,
} = require("../src/jsonRpcClient");

class FakeProcess extends EventEmitter {
  constructor() {
    super();
    this.stdin = new PassThrough();
    this.stdout = new PassThrough();
    this.stderr = new PassThrough();
  }
}

function decodeSingle(buffer) {
  const separator = buffer.indexOf("\r\n\r\n");
  assert.notEqual(separator, -1);
  return JSON.parse(buffer.subarray(separator + 4).toString("utf8"));
}

test("message reader handles fragmented and consecutive frames", () => {
  const messages = [];
  const reader = new MessageReader((message) => messages.push(message));
  const first = encodeMessage({ id: 1, value: "first" });
  const second = encodeMessage({ id: 2, value: "second" });
  const combined = Buffer.concat([first, second]);

  reader.push(combined.subarray(0, 7));
  reader.push(combined.subarray(7, first.length + 3));
  reader.push(combined.subarray(first.length + 3));

  assert.deepEqual(messages, [
    { id: 1, value: "first" },
    { id: 2, value: "second" },
  ]);
});

test("client correlates successful and failed responses", async () => {
  const process = new FakeProcess();
  const client = new JsonRpcClient(process);
  const writes = [];
  process.stdin.on("data", (chunk) => writes.push(chunk));

  const success = client.sendRequest("workspace/status");
  assert.equal(decodeSingle(writes[0]).method, "workspace/status");
  process.stdout.write(
    encodeMessage({ jsonrpc: "2.0", id: success.id, result: { initialized: true } }),
  );
  assert.deepEqual(await success.promise, { initialized: true });

  const failure = client.sendRequest("query/symbol", { name: "missing" });
  process.stdout.write(
    encodeMessage({
      jsonrpc: "2.0",
      id: failure.id,
      error: { code: -32012, message: "not found", data: { sherpa_code: "not_found" } },
    }),
  );
  await assert.rejects(failure.promise, (error) => {
    assert.ok(error instanceof JsonRpcError);
    assert.equal(error.code, -32012);
    assert.equal(error.data.sherpa_code, "not_found");
    return true;
  });
});

test("client emits JSON-RPC cancellation notifications", () => {
  const process = new FakeProcess();
  const client = new JsonRpcClient(process);
  const writes = [];
  process.stdin.on("data", (chunk) => writes.push(chunk));

  const request = client.sendRequest("workspace/index");
  request.cancel();

  assert.equal(writes.length, 2);
  assert.deepEqual(decodeSingle(writes[1]), {
    jsonrpc: "2.0",
    method: "$/cancelRequest",
    params: { id: request.id },
  });
  client.dispose();
  void request.promise.catch(() => {});
});

test("disposing rejects pending work and prevents new requests", async () => {
  const process = new FakeProcess();
  const client = new JsonRpcClient(process);
  const request = client.sendRequest("workspace/index");
  const rejection = assert.rejects(request.promise, /disposed/);

  client.dispose();

  await rejection;
  assert.throws(() => client.sendRequest("workspace/status"), /disposed/);
});
