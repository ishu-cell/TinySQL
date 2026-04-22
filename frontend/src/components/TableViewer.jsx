import { useState, useEffect } from 'react';
import { selectAll } from '../api';

const PAGE_SIZE = 10;

export default function TableViewer({ refreshKey }) {
  const [rows, setRows] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [page, setPage] = useState(0);
  const [sortField, setSortField] = useState('id');
  const [sortDir, setSortDir] = useState('asc');

  const fetchData = async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await selectAll();
      if (result.status === 'success') {
        setRows(result.rows || []);
      } else {
        setError(result.message || 'Failed to fetch rows.');
      }
    } catch (err) {
      setError(`Network error: ${err.message}`);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
  }, [refreshKey]);

  // Sorting
  const sorted = [...rows].sort((a, b) => {
    let cmp = 0;
    if (sortField === 'id') cmp = a.id - b.id;
    else cmp = (a[sortField] || '').localeCompare(b[sortField] || '');
    return sortDir === 'asc' ? cmp : -cmp;
  });

  // Pagination
  const totalPages = Math.max(1, Math.ceil(sorted.length / PAGE_SIZE));
  const pageRows = sorted.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE);

  const toggleSort = (field) => {
    if (sortField === field) setSortDir((d) => (d === 'asc' ? 'desc' : 'asc'));
    else { setSortField(field); setSortDir('asc'); }
  };

  const SortArrow = ({ field }) => {
    if (sortField !== field) return <span className="opacity-20 ml-1">↕</span>;
    return <span className="text-matrix ml-1">{sortDir === 'asc' ? '↑' : '↓'}</span>;
  };

  return (
    <div className="glass-panel animate-slide-up">
      {/* Header */}
      <div className="flex items-center justify-between px-5 py-3 border-b border-border-g">
        <div className="flex items-center gap-3">
          <h2 className="text-sm font-semibold text-[var(--text-light)]">
            <span className="text-matrix mr-2">⊞</span>Table Viewer
          </h2>
          <span className="text-xs text-[var(--text-dim)] bg-bg-panel px-2 py-0.5 rounded">
            {rows.length} row{rows.length !== 1 ? 's' : ''}
          </span>
        </div>
        <button
          id="refresh-table"
          onClick={fetchData}
          disabled={loading}
          className="btn-primary px-3 py-1.5 rounded-lg text-xs font-medium"
        >
          {loading ? 'Loading…' : '↻ Refresh'}
        </button>
      </div>

      {/* Error */}
      {error && (
        <div className="mx-5 mt-3 px-3 py-2 rounded-lg bg-err/10 border border-err/20 text-err text-xs">
          {error}
        </div>
      )}

      {/* Table */}
      <div className="overflow-x-auto p-4">
        <table className="w-full text-sm">
          <thead>
            <tr className="text-[var(--text-dim)] text-xs uppercase tracking-wider border-b border-border-g">
              {['id', 'username', 'email'].map((field) => (
                <th
                  key={field}
                  onClick={() => toggleSort(field)}
                  className="py-2.5 px-3 text-left cursor-pointer hover:text-matrix transition-colors select-none"
                >
                  {field}
                  <SortArrow field={field} />
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {pageRows.length === 0 ? (
              <tr>
                <td colSpan={3} className="py-12 text-center text-[var(--text-dim)]">
                  {loading ? (
                    <span className="dot-pulse">Loading<span>.</span><span>.</span><span>.</span></span>
                  ) : (
                    <>No rows found. Insert some data first.</>
                  )}
                </td>
              </tr>
            ) : (
              pageRows.map((row, i) => (
                <tr
                  key={row.id}
                  className="row-glow border-b border-border-g/50 transition-all duration-150"
                  style={{ animationDelay: `${i * 30}ms` }}
                >
                  <td className="py-2.5 px-3 text-matrix font-medium">{row.id}</td>
                  <td className="py-2.5 px-3 text-cyan-acc">{row.username}</td>
                  <td className="py-2.5 px-3 text-[var(--text-mid)]">{row.email}</td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* Pagination */}
      {totalPages > 1 && (
        <div className="flex items-center justify-between px-5 py-3 border-t border-border-g text-xs">
          <span className="text-[var(--text-dim)]">
            Page {page + 1} of {totalPages}
          </span>
          <div className="flex gap-2">
            <button
              onClick={() => setPage((p) => Math.max(0, p - 1))}
              disabled={page === 0}
              className="px-3 py-1 rounded bg-bg-panel border border-border-g text-[var(--text-mid)] hover:text-matrix hover:border-matrix/30 transition-colors disabled:opacity-30"
            >
              ← Prev
            </button>
            <button
              onClick={() => setPage((p) => Math.min(totalPages - 1, p + 1))}
              disabled={page >= totalPages - 1}
              className="px-3 py-1 rounded bg-bg-panel border border-border-g text-[var(--text-mid)] hover:text-matrix hover:border-matrix/30 transition-colors disabled:opacity-30"
            >
              Next →
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
