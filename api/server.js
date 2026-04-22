/*
 * server.js — TinySQL REST API Bridge
 *
 * Spawns the C++ tinysql engine as a child process and communicates
 * via stdin/stdout.  Exposes HTTP endpoints for the frontend.
 *
 * Commands are serialized (one at a time) via a promise queue to
 * ensure thread safety.
 */

const express = require('express');
const cors    = require('cors');
const { spawn } = require('child_process');
const path    = require('path');
const readline = require('readline');

const app  = express();
const PORT = 3001;

app.use(cors());
app.use(express.json());

// ============================================================
//  Spawn the C++ engine
// ============================================================

const ENGINE_PATH = path.join(__dirname, '..', 'engine', 'tinysql.exe');
const DB_PATH     = path.join(__dirname, '..', 'engine', 'tinysql.db');

let engine = null;
let rl     = null;  // readline interface on engine's stdout

// Queue for serialising commands (one at a time)
let commandQueue = Promise.resolve();
let engineReady  = false;

function startEngine() {
    console.log(`[TinySQL API] Starting engine: ${ENGINE_PATH} ${DB_PATH} --json`);

    engine = spawn(ENGINE_PATH, [DB_PATH, '--json'], {
        stdio: ['pipe', 'pipe', 'pipe'],
    });

    // Read stderr for debug / error messages
    engine.stderr.on('data', (data) => {
        console.error(`[engine stderr] ${data.toString().trim()}`);
    });

    engine.on('error', (err) => {
        console.error(`[engine error] ${err.message}`);
        engineReady = false;
    });

    engine.on('close', (code) => {
        console.log(`[engine] Process exited with code ${code}`);
        engineReady = false;
        engine = null;
    });

    // Setup readline on stdout for line-based JSON parsing
    rl = readline.createInterface({ input: engine.stdout });

    // Wait for the "ready" message
    return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
            reject(new Error('Engine did not become ready in time'));
        }, 10000);

        rl.once('line', (line) => {
            clearTimeout(timeout);
            try {
                const msg = JSON.parse(line);
                if (msg.status === 'ready') {
                    engineReady = true;
                    console.log(`[TinySQL API] Engine ready: ${msg.message}`);
                    resolve();
                } else {
                    reject(new Error(`Unexpected first message: ${line}`));
                }
            } catch (e) {
                reject(new Error(`Failed to parse engine greeting: ${line}`));
            }
        });
    });
}

/**
 * Send a command to the engine and wait for exactly one line of JSON response.
 * Commands are serialised through a promise queue.
 */
function sendCommand(command) {
    return new Promise((resolve, reject) => {
        commandQueue = commandQueue.then(() => {
            return new Promise((res) => {
                if (!engineReady || !engine) {
                    resolve({ status: 'error', message: 'Engine is not running.' });
                    res();
                    return;
                }

                const timeout = setTimeout(() => {
                    resolve({ status: 'error', message: 'Engine timed out.' });
                    res();
                }, 15000);

                rl.once('line', (line) => {
                    clearTimeout(timeout);
                    try {
                        resolve(JSON.parse(line));
                    } catch (e) {
                        resolve({ status: 'error', message: `Bad JSON from engine: ${line}` });
                    }
                    res();
                });

                engine.stdin.write(command + '\n');
            });
        });
    });
}

// ============================================================
//  REST Endpoints
// ============================================================

app.get('/api/health', (_req, res) => {
    res.json({ status: engineReady ? 'ok' : 'error', engineRunning: engineReady });
});

app.post('/api/insert', async (req, res) => {
    const { id, username, email } = req.body;
    if (id === undefined || !username || !email) {
        return res.status(400).json({ status: 'error', message: 'Missing fields: id, username, email' });
    }
    const result = await sendCommand(`insert ${id} ${username} ${email}`);
    res.json(result);
});

app.get('/api/select', async (_req, res) => {
    const result = await sendCommand('select');
    res.json(result);
});

app.get('/api/find', async (req, res) => {
    const { id } = req.query;
    if (id === undefined) {
        return res.status(400).json({ status: 'error', message: 'Missing query parameter: id' });
    }
    const result = await sendCommand(`find ${id}`);
    res.json(result);
});

app.get('/api/count', async (_req, res) => {
    const result = await sendCommand('count');
    res.json(result);
});

app.post('/api/execute', async (req, res) => {
    const { command } = req.body;
    if (!command) {
        return res.status(400).json({ status: 'error', message: 'Missing field: command' });
    }
    // Prevent .exit through the API (would kill the engine)
    if (command.trim() === '.exit') {
        return res.json({ status: 'error', message: 'Cannot exit engine via API.' });
    }
    const result = await sendCommand(command);
    res.json(result);
});

// ============================================================
//  Start
// ============================================================

(async () => {
    try {
        await startEngine();
        app.listen(PORT, () => {
            console.log(`[TinySQL API] Server listening on http://localhost:${PORT}`);
        });
    } catch (err) {
        console.error(`[TinySQL API] Failed to start: ${err.message}`);
        process.exit(1);
    }
})();
