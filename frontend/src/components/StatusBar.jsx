import { useState, useEffect } from 'react';
import { getHealth, getCount } from '../api';

export default function StatusBar() {
  const [connected, setConnected] = useState(false);
  const [rowCount, setRowCount] = useState(null);
  const [checking, setChecking] = useState(true);

  const checkStatus = async () => {
    setChecking(true);
    try {
      const [health, count] = await Promise.all([getHealth(), getCount()]);
      setConnected(health.status === 'ok');
      if (count.status === 'success') setRowCount(count.count);
    } catch {
      setConnected(false);
    } finally {
      setChecking(false);
    }
  };

  useEffect(() => {
    checkStatus();
    const interval = setInterval(checkStatus, 10000); // poll every 10s
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="flex items-center gap-4 text-xs">
      {/* Connection Status */}
      <div className="flex items-center gap-1.5">
        <span
          className={`w-2 h-2 rounded-full ${
            checking
              ? 'bg-warn animate-pulse'
              : connected
                ? 'bg-matrix animate-pulse-slow'
                : 'bg-err'
          }`}
        />
        <span className={connected ? 'text-matrix' : 'text-err'}>
          {checking ? 'Connecting…' : connected ? 'Engine Online' : 'Disconnected'}
        </span>
      </div>

      {/* Row Count */}
      {rowCount !== null && (
        <div className="hidden sm:flex items-center gap-1.5 text-[var(--text-dim)]">
          <span className="text-[var(--text-dim)]">│</span>
          <span>{rowCount} row{rowCount !== 1 ? 's' : ''}</span>
        </div>
      )}

      {/* Refresh */}
      <button
        onClick={checkStatus}
        className="text-[var(--text-dim)] hover:text-matrix transition-colors"
        title="Refresh status"
      >
        ↻
      </button>
    </div>
  );
}
