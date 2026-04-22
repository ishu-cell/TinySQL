import { useState } from 'react';
import { findById } from '../api';

export default function SearchPanel() {
  const [searchId, setSearchId] = useState('');
  const [result, setResult] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(false);

  const handleSearch = async (e) => {
    e.preventDefault();
    const id = searchId.trim();
    if (!id) return;

    setLoading(true);
    setResult(null);
    setError(null);

    try {
      const data = await findById(id);
      if (data.status === 'success' && data.row) {
        setResult(data.row);
      } else {
        setError(data.message || 'Row not found.');
      }
    } catch (err) {
      setError(`Network error: ${err.message}`);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="glass-panel animate-slide-up max-w-xl mx-auto">
      {/* Header */}
      <div className="px-5 py-3 border-b border-border-g">
        <h2 className="text-sm font-semibold text-[var(--text-light)]">
          <span className="text-matrix mr-2">⌕</span>B+ Tree Lookup
        </h2>
        <p className="text-xs text-[var(--text-dim)] mt-1">
          O(log n) indexed search by primary key
        </p>
      </div>

      {/* Search Input */}
      <form onSubmit={handleSearch} className="p-5">
        <label className="text-xs text-[var(--text-dim)] uppercase tracking-wider mb-2 block">
          Row ID
        </label>
        <div className="flex gap-3">
          <input
            id="search-id-input"
            type="number"
            min="0"
            value={searchId}
            onChange={(e) => setSearchId(e.target.value)}
            placeholder="Enter ID…"
            className="flex-1 bg-bg-deep border border-border-g rounded-lg px-4 py-2.5 text-sm text-[var(--text-light)] placeholder:text-[var(--text-dim)] font-mono focus:border-matrix/40 transition-colors"
          />
          <button
            id="search-btn"
            type="submit"
            disabled={loading || !searchId.trim()}
            className="btn-primary px-5 py-2.5 rounded-lg text-sm font-medium disabled:opacity-30"
          >
            {loading ? '…' : 'Find'}
          </button>
        </div>
      </form>

      {/* Result */}
      {result && (
        <div className="mx-5 mb-5 animate-slide-up">
          <div className="bg-bg-deep rounded-lg border border-matrix/20 overflow-hidden">
            <div className="px-4 py-2 border-b border-border-g text-xs text-matrix font-medium">
              ✓ Row Found
            </div>
            <div className="p-4 space-y-3">
              <div className="flex items-center justify-between">
                <span className="text-xs text-[var(--text-dim)] uppercase tracking-wider">ID</span>
                <span className="text-matrix font-bold text-lg">{result.id}</span>
              </div>
              <div className="w-full h-px bg-border-g"></div>
              <div className="flex items-center justify-between">
                <span className="text-xs text-[var(--text-dim)] uppercase tracking-wider">Username</span>
                <span className="text-cyan-acc font-medium">{result.username}</span>
              </div>
              <div className="w-full h-px bg-border-g"></div>
              <div className="flex items-center justify-between">
                <span className="text-xs text-[var(--text-dim)] uppercase tracking-wider">Email</span>
                <span className="text-[var(--text-mid)]">{result.email}</span>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Error */}
      {error && (
        <div className="mx-5 mb-5 px-4 py-3 rounded-lg bg-err/10 border border-err/20 text-err text-sm animate-fade-in">
          ✗ {error}
        </div>
      )}
    </div>
  );
}
