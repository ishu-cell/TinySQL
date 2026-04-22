/*
 * api.js — TinySQL API client
 *
 * Uses the Vite dev-server proxy (/api → localhost:3001)
 * so no CORS issues during development.
 */

const API_BASE = '/api';

export async function insertRow(id, username, email) {
  const res = await fetch(`${API_BASE}/insert`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id: Number(id), username, email }),
  });
  return res.json();
}

export async function selectAll() {
  const res = await fetch(`${API_BASE}/select`);
  return res.json();
}

export async function findById(id) {
  const res = await fetch(`${API_BASE}/find?id=${id}`);
  return res.json();
}

export async function getCount() {
  const res = await fetch(`${API_BASE}/count`);
  return res.json();
}

export async function executeCommand(command) {
  const res = await fetch(`${API_BASE}/execute`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ command }),
  });
  return res.json();
}

export async function getHealth() {
  try {
    const res = await fetch(`${API_BASE}/health`);
    return res.json();
  } catch {
    return { status: 'error', engineRunning: false };
  }
}
