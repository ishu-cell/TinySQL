import { useState, useRef, useEffect } from 'react';
import { executeCommand } from '../api';

export default function QueryConsole({ onDataChange }) {
  const [history, setHistory] = useState([
    { type: 'system', text: 'TinySQL Interactive Console — Type SQL-like commands below.' },
    { type: 'system', text: 'Commands: insert <id> <user> <email> | select | find <id> | count' },
  ]);
  const [input, setInput] = useState('');
  const [loading, setLoading] = useState(false);
  const [cmdHistory, setCmdHistory] = useState([]);
  const [historyIdx, setHistoryIdx] = useState(-1);
  const outputRef = useRef(null);
  const inputRef = useRef(null);

  // Auto-scroll output to bottom
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [history]);

  const handleSubmit = async (e) => {
    e.preventDefault();
    const cmd = input.trim();
    if (!cmd) return;

    // Add user command to history
    setHistory((h) => [...h, { type: 'user', text: cmd }]);
    setCmdHistory((h) => [cmd, ...h]);
    setHistoryIdx(-1);
    setInput('');
    setLoading(true);

    try {
      const result = await executeCommand(cmd);
      let outputLines = [];

      if (result.status === 'success') {
        if (result.rows) {
          if (result.rows.length === 0) {
            outputLines.push({ type: 'info', text: '(empty table)' });
          } else {
            result.rows.forEach((row) => {
              outputLines.push({
                type: 'result',
                text: `(${row.id}, ${row.username}, ${row.email})`,
              });
            });
            outputLines.push({ type: 'info', text: `${result.rows.length} row(s) returned.` });
          }
        } else if (result.row) {
          outputLines.push({
            type: 'result',
            text: `(${result.row.id}, ${result.row.username}, ${result.row.email})`,
          });
        } else if (result.count !== undefined) {
          outputLines.push({ type: 'result', text: `Count: ${result.count}` });
        } else if (result.message) {
          outputLines.push({ type: 'success', text: result.message });
        }
        // Trigger refresh for insert/delete operations
        if (cmd.startsWith('insert')) onDataChange?.();
      } else {
        outputLines.push({ type: 'error', text: result.message || 'Unknown error.' });
      }

      setHistory((h) => [...h, ...outputLines]);
    } catch (err) {
      setHistory((h) => [
        ...h,
        { type: 'error', text: `Network error: ${err.message}` },
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleKeyDown = (e) => {
    if (e.key === 'ArrowUp') {
      e.preventDefault();
      const newIdx = Math.min(historyIdx + 1, cmdHistory.length - 1);
      setHistoryIdx(newIdx);
      if (cmdHistory[newIdx]) setInput(cmdHistory[newIdx]);
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      const newIdx = Math.max(historyIdx - 1, -1);
      setHistoryIdx(newIdx);
      setInput(newIdx >= 0 ? cmdHistory[newIdx] : '');
    }
  };

  const getLineColor = (type) => {
    switch (type) {
      case 'user':    return 'text-cyan-acc';
      case 'result':  return 'text-[var(--text-light)]';
      case 'success': return 'text-matrix';
      case 'error':   return 'text-err';
      case 'info':    return 'text-[var(--text-dim)]';
      case 'system':  return 'text-warn opacity-70';
      default:        return 'text-[var(--text-mid)]';
    }
  };

  const getLinePrefix = (type) => {
    switch (type) {
      case 'user':    return 'tinysql> ';
      case 'result':  return '  ';
      case 'success': return '✓ ';
      case 'error':   return '✗ ';
      case 'info':    return '  ';
      case 'system':  return '# ';
      default:        return '';
    }
  };

  return (
    <div className="glass-panel overflow-hidden animate-slide-up">
      {/* Terminal Header */}
      <div className="flex items-center gap-2 px-4 py-2.5 border-b border-border-g bg-bg-card/60">
        <div className="flex gap-1.5">
          <span className="w-3 h-3 rounded-full bg-err/80"></span>
          <span className="w-3 h-3 rounded-full bg-warn/80"></span>
          <span className="w-3 h-3 rounded-full bg-matrix/80"></span>
        </div>
        <span className="text-xs text-[var(--text-dim)] ml-2">tinysql — interactive console</span>
      </div>

      {/* Terminal Output */}
      <div
        ref={outputRef}
        className="terminal-output scanline-overlay p-4 h-[400px] md:h-[500px] overflow-y-auto font-mono text-sm leading-relaxed bg-bg-deep/80"
      >
        {history.map((line, i) => (
          <div key={i} className={`${getLineColor(line.type)} whitespace-pre-wrap animate-fade-in`}>
            <span className="opacity-40 select-none">{getLinePrefix(line.type)}</span>
            {line.text}
          </div>
        ))}
        {loading && (
          <div className="text-matrix dot-pulse">
            Processing<span>.</span><span>.</span><span>.</span>
          </div>
        )}
      </div>

      {/* Input Line */}
      <form onSubmit={handleSubmit} className="flex items-center border-t border-border-g bg-bg-card/40">
        <span className="text-matrix pl-4 pr-1 text-sm select-none">❯</span>
        <input
          ref={inputRef}
          id="console-input"
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="Type a command…"
          disabled={loading}
          autoFocus
          className="flex-1 bg-transparent py-3 px-2 text-sm text-[var(--text-light)] placeholder:text-[var(--text-dim)] font-mono border-none focus:ring-0"
        />
        <button
          type="submit"
          disabled={loading || !input.trim()}
          className="px-4 py-3 text-matrix text-sm font-medium hover:bg-matrix/10 transition-colors disabled:opacity-30"
        >
          Run ↵
        </button>
      </form>
    </div>
  );
}
