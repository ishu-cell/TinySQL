import { useState, useEffect } from 'react';
import QueryConsole   from './components/QueryConsole';
import TableViewer    from './components/TableViewer';
import SearchPanel    from './components/SearchPanel';
import InsertForm     from './components/InsertForm';
import StatusBar      from './components/StatusBar';

const TABS = [
  { id: 'console', label: '> Console',  icon: '⌘' },
  { id: 'table',   label: 'Table View', icon: '⊞' },
  { id: 'search',  label: 'Search',     icon: '⌕' },
  { id: 'insert',  label: 'Insert',     icon: '+' },
];

export default function App() {
  const [activeTab, setActiveTab] = useState('console');
  const [refreshKey, setRefreshKey] = useState(0);

  // Allow child components to trigger a data refresh
  const triggerRefresh = () => setRefreshKey((k) => k + 1);

  return (
    <div className="min-h-screen flex flex-col bg-bg-deep">
      {/* ── Header ── */}
      <header className="flex items-center justify-between px-6 py-3 border-b border-border-g bg-bg-card/80 backdrop-blur-md">
        <div className="flex items-center gap-3">
          <div className="relative">
            <span className="text-2xl font-bold gradient-text tracking-tight">⚡ TinySQL</span>
          </div>
          <span className="hidden sm:inline text-xs text-[var(--text-dim)] mt-1 tracking-wide">
            Disk-Persistent B+ Tree Engine
          </span>
        </div>
        <StatusBar />
      </header>

      {/* ── Tab Navigation ── */}
      <nav className="flex gap-0 border-b border-border-g bg-bg-card/40">
        {TABS.map((tab) => (
          <button
            key={tab.id}
            id={`tab-${tab.id}`}
            onClick={() => setActiveTab(tab.id)}
            className={`
              relative px-5 py-3 text-sm font-medium transition-all duration-200
              ${activeTab === tab.id
                ? 'text-matrix tab-active bg-bg-panel/50'
                : 'text-[var(--text-dim)] hover:text-[var(--text-mid)] hover:bg-bg-panel/20'
              }
            `}
          >
            <span className="mr-2 opacity-60">{tab.icon}</span>
            {tab.label}
          </button>
        ))}
      </nav>

      {/* ── Main Content ── */}
      <main className="flex-1 p-4 md:p-6 animate-fade-in overflow-auto">
        <div className="max-w-6xl mx-auto">
          {activeTab === 'console' && <QueryConsole onDataChange={triggerRefresh} />}
          {activeTab === 'table'   && <TableViewer refreshKey={refreshKey} />}
          {activeTab === 'search'  && <SearchPanel />}
          {activeTab === 'insert'  && <InsertForm onInserted={triggerRefresh} />}
        </div>
      </main>

      {/* ── Footer ── */}
      <footer className="px-6 py-2 text-center text-[10px] text-[var(--text-dim)] border-t border-border-g">
        TinySQL v1.0 — B+ Tree Engine with {13} keys/leaf · {510} keys/internal · 4KB pages
      </footer>
    </div>
  );
}
