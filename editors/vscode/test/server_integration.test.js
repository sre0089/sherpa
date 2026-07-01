"use strict";

const assert = require("node:assert/strict");
const { spawn } = require("node:child_process");
const fs = require("node:fs");
const test = require("node:test");

const { JsonRpcClient } = require("../src/jsonRpcClient");

test(
  "client indexes and queries through the built sherpa-server",
  { timeout: 15000 },
  async () => {
    const serverPath = process.env.SHERPA_SERVER_PATH;
    const repositoryPath = process.env.SHERPA_TEST_REPOSITORY;
    const databasePath = process.env.SHERPA_TEST_DATABASE;
    assert.ok(serverPath);
    assert.ok(repositoryPath);
    assert.ok(databasePath);
    fs.rmSync(databasePath, { force: true });

    const child = spawn(serverPath, [], {
      stdio: ["pipe", "pipe", "pipe"],
      windowsHide: true,
    });
    let stderr = "";
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString("utf8");
    });
    const client = new JsonRpcClient(child);

    try {
      const initialized = await client.sendRequest("initialize", {
        repository_path: repositoryPath,
        database_path: databasePath,
      }).promise;
      assert.equal(initialized.protocol_version, 1);

      const indexed = await client.sendRequest("workspace/index").promise;
      assert.equal(indexed.indexed_files, 3);

      const symbol = await client.sendRequest("query/symbol", { name: "add" }).promise;
      assert.equal(symbol.schema_version, 1);
      assert.equal(symbol.symbol.qualified_name, "Calculator::add");

      await client.sendRequest("shutdown").promise;
      client.notify("exit");
      child.stdin.end();
      const exitCode = await new Promise((resolve) => child.once("exit", resolve));
      assert.equal(exitCode, 0, stderr);
    } finally {
      client.dispose();
      if (child.exitCode === null) {
        child.kill();
      }
      fs.rmSync(databasePath, { force: true });
    }
  },
);
