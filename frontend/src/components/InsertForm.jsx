import { useState } from 'react';
import { insertRow } from '../api';

export default function InsertForm({ onInserted }) {
  const [id, setId] = useState('');
  const [username, setUsername] = useState('');
  const [email, setEmail] = useState('');
  const [status, setStatus] = useState(null); // { type: 'success'|'error', message }
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!id || !username || !email) return;

    setLoading(true);
    setStatus(null);

    try {
      const result = await insertRow(id, username.trim(), email.trim());
      if (result.status === 'success') {
        setStatus({ type: 'success', message: result.message || 'Row inserted successfully.' });
        setId('');
        setUsername('');
        setEmail('');
        onInserted?.();
      } else {
        setStatus({ type: 'error', message: result.message || 'Insert failed.' });
      }
    } catch (err) {
      setStatus({ type: 'error', message: `Network error: ${err.message}` });
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="glass-panel animate-slide-up max-w-xl mx-auto">
      {/* Header */}
      <div className="px-5 py-3 border-b border-border-g">
        <h2 className="text-sm font-semibold text-[var(--text-light)]">
          <span className="text-matrix mr-2">+</span>Insert Row
        </h2>
        <p className="text-xs text-[var(--text-dim)] mt-1">
          Add a new record to the B+ Tree
        </p>
      </div>

      {/* Form */}
      <form onSubmit={handleSubmit} className="p-5 space-y-4">
        {/* ID */}
        <div>
          <label htmlFor="insert-id" className="text-xs text-[var(--text-dim)] uppercase tracking-wider mb-1.5 block">
            ID <span className="text-err">*</span>
          </label>
          <input
            id="insert-id"
            type="number"
            min="0"
            value={id}
            onChange={(e) => setId(e.target.value)}
            placeholder="Unique integer key"
            required
            className="w-full bg-bg-deep border border-border-g rounded-lg px-4 py-2.5 text-sm text-[var(--text-light)] placeholder:text-[var(--text-dim)] font-mono focus:border-matrix/40 transition-colors"
          />
        </div>

        {/* Username */}
        <div>
          <label htmlFor="insert-username" className="text-xs text-[var(--text-dim)] uppercase tracking-wider mb-1.5 block">
            Username <span className="text-err">*</span>
            <span className="text-[var(--text-dim)] ml-2 normal-case">(max 32 chars)</span>
          </label>
          <input
            id="insert-username"
            type="text"
            maxLength={32}
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            placeholder="alice"
            required
            className="w-full bg-bg-deep border border-border-g rounded-lg px-4 py-2.5 text-sm text-[var(--text-light)] placeholder:text-[var(--text-dim)] font-mono focus:border-matrix/40 transition-colors"
          />
          <div className="flex justify-end mt-1">
            <span className={`text-[10px] ${username.length > 28 ? 'text-warn' : 'text-[var(--text-dim)]'}`}>
              {username.length}/32
            </span>
          </div>
        </div>

        {/* Email */}
        <div>
          <label htmlFor="insert-email" className="text-xs text-[var(--text-dim)] uppercase tracking-wider mb-1.5 block">
            Email <span className="text-err">*</span>
            <span className="text-[var(--text-dim)] ml-2 normal-case">(max 255 chars)</span>
          </label>
          <input
            id="insert-email"
            type="text"
            maxLength={255}
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            placeholder="alice@example.com"
            required
            className="w-full bg-bg-deep border border-border-g rounded-lg px-4 py-2.5 text-sm text-[var(--text-light)] placeholder:text-[var(--text-dim)] font-mono focus:border-matrix/40 transition-colors"
          />
          <div className="flex justify-end mt-1">
            <span className={`text-[10px] ${email.length > 240 ? 'text-warn' : 'text-[var(--text-dim)]'}`}>
              {email.length}/255
            </span>
          </div>
        </div>

        {/* Submit */}
        <button
          id="insert-submit"
          type="submit"
          disabled={loading || !id || !username || !email}
          className="w-full btn-primary py-3 rounded-lg text-sm font-semibold tracking-wide disabled:opacity-30"
        >
          {loading ? (
            <span className="dot-pulse">Inserting<span>.</span><span>.</span><span>.</span></span>
          ) : (
            '⚡ Insert Row'
          )}
        </button>
      </form>

      {/* Status Feedback */}
      {status && (
        <div className={`mx-5 mb-5 px-4 py-3 rounded-lg text-sm animate-fade-in ${
          status.type === 'success'
            ? 'bg-matrix/10 border border-matrix/20 text-matrix'
            : 'bg-err/10 border border-err/20 text-err'
        }`}>
          {status.type === 'success' ? '✓' : '✗'} {status.message}
        </div>
      )}
    </div>
  );
}
