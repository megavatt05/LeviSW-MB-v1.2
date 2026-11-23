import React, { useMemo, useState } from 'react';
import {
  Search,
  LayoutGrid,
  Table as TableIcon,
  ExternalLink,
  RotateCcw,
  ArrowRight,
  Cpu,
  Radio,
  Grid3X3,
  Zap,
  Layers,
  AlertCircle,
} from 'lucide-react';
import {
  SCHEMATIC_NODES,
  type NodeCategory,
  type SchematicNode,
} from '../data/schematicData';

interface NodesPageProps {
  onOpenSheet: (pageNumber: number) => void;
}

const CATEGORIES: Array<'Все' | NodeCategory> = [
  'Все',
  'Интерфейсы и связь',
  'Ядро МК',
  'Матричная коммутация',
  'Питание и телеметрия',
  'Конструкция ПП',
];

const STRUCTURAL_GROUPS: Array<{
  title: string;
  subtitle: string;
  nodeIds: number[];
  accentClass: string;
  icon: React.ReactNode;
}> = [
  {
    title: 'Магистраль и внешние интерфейсы',
    subtitle: 'Клеммы, Левитан, RS-485, LU1/LU2, 1-Wire',
    nodeIds: [1, 2, 3, 4, 5],
    accentClass: 'text-cyan-400',
    icon: <Radio className="w-4 h-4 text-cyan-400" />,
  },
  {
    title: 'Вычислительное ядро МК',
    subtitle: 'ESP32-PICO-V3, расширитель I2C, АЦП и отладка',
    nodeIds: [6, 7, 8],
    accentClass: 'text-indigo-400',
    icon: <Cpu className="w-4 h-4 text-indigo-400" />,
  },
  {
    title: 'Матричная коммутация 8×10',
    subtitle: 'Десятки D0–D7, смещение VOffset, единицы E0–E9',
    nodeIds: [9, 10, 11],
    accentClass: 'text-violet-400',
    icon: <Grid3X3 className="w-4 h-4 text-violet-400" />,
  },
  {
    title: 'Питание и телеметрия',
    subtitle: 'Защита POWERIN, DC-DC и каналы MV1/MV1e/MV2',
    nodeIds: [12, 13, 14, 15],
    accentClass: 'text-emerald-400',
    icon: <Zap className="w-4 h-4 text-emerald-400" />,
  },
  {
    title: 'Конструкция печатной платы',
    subtitle: 'Механика, стек FR4 4L, сборка 1:1 и Gerber',
    nodeIds: [16],
    accentClass: 'text-amber-400',
    icon: <Layers className="w-4 h-4 text-amber-400" />,
  },
];

function getQuickSheets(pages: number[]): number[] {
  if (pages.length <= 4) return pages;
  // Для узла 16, охватывающего листы 13..23, выделить основные справочные листы
  return [13, 14, 17, 21];
}

export default function NodesPage({ onOpenSheet }: NodesPageProps) {
  const [searchQuery, setSearchQuery] = useState<string>('');
  const [selectedCategory, setSelectedCategory] = useState<'Все' | NodeCategory>('Все');
  const [viewMode, setViewMode] = useState<'cards' | 'table'>('cards');
  const [highlightedNodeId, setHighlightedNodeId] = useState<number | null>(null);

  const categoryCounts = useMemo(() => {
    const counts: Record<string, number> = { Все: SCHEMATIC_NODES.length };
    for (const node of SCHEMATIC_NODES) {
      counts[node.category] = (counts[node.category] || 0) + 1;
    }
    return counts;
  }, []);

  const filteredNodes = useMemo(() => {
    const q = searchQuery.trim().toLowerCase();
    return SCHEMATIC_NODES.filter((node) => {
      const matchesCategory =
        selectedCategory === 'Все' || node.category === selectedCategory;
      if (!matchesCategory) return false;
      if (!q) return true;

      const haystack = [
        node.code,
        String(node.id).padStart(2, '0'),
        node.title,
        node.schematicLabel,
        node.category,
        node.sheetName,
        node.description,
        node.notes || '',
        ...node.components,
        ...node.signalsIn,
        ...node.signalsOut,
        ...node.testPoints,
      ]
        .join(' ')
        .toLowerCase();

      return haystack.includes(q);
    });
  }, [searchQuery, selectedCategory]);

  const handleJumpToNode = (nodeId: number) => {
    const isVisible = filteredNodes.some((n) => n.id === nodeId);
    if (!isVisible) {
      setSearchQuery('');
      setSelectedCategory('Все');
    }
    setHighlightedNodeId(nodeId);
    window.setTimeout(() => {
      const el = document.getElementById(`schematic-node-${nodeId}`);
      if (el) {
        el.scrollIntoView({ behavior: 'smooth', block: 'center' });
      }
    }, 60);
  };

  const resetFilters = () => {
    setSearchQuery('');
    setSelectedCategory('Все');
    setHighlightedNodeId(null);
  };

  return (
    <div className="space-y-6">
      {/* Заголовок и панель поиска */}
      <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl space-y-5">
        <div className="flex flex-col lg:flex-row lg:items-center justify-between gap-4">
          <div>
            <div className="flex items-center space-x-2 text-xs font-mono text-indigo-400 mb-1">
              <span>КАРТА ФУНКЦИОНАЛЬНЫХ БЛОКОВ</span>
              <span className="text-slate-600">·</span>
              <span className="tabular-nums">16 УЗЛОВ</span>
              <span className="text-slate-600">·</span>
              <span className="tabular-nums">TP1–TP201</span>
            </div>
            <h1 className="text-2xl lg:text-3xl font-bold text-white tracking-tight">
              Нумерованные узлы принципиальной схемы
            </h1>
            <p className="text-slate-400 text-sm mt-1 max-w-3xl">
              Структурная декомпозиция материнской платы LeviSW-MB v1.2 от входных клемм до матрицы абонентов 8×10, подсистемы питания и чертежей печатной платы.
            </p>
          </div>

          <div className="flex flex-col sm:flex-row items-stretch sm:items-center gap-3 shrink-0">
            <div className="relative w-full sm:w-72">
              <Search className="w-4 h-4 text-slate-400 absolute left-3.5 top-3" />
              <input
                type="text"
                placeholder="Поиск по узлу, ИС, цепи, TP..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full bg-slate-950 border border-slate-700 text-slate-200 pl-10 pr-8 py-2.5 rounded-xl text-sm focus:outline-none focus:border-indigo-500"
              />
              {searchQuery && (
                <button
                  type="button"
                  onClick={() => setSearchQuery('')}
                  className="absolute right-2.5 top-2.5 text-xs text-slate-400 hover:text-white px-1"
                  title="Очистить поиск"
                >
                  ×
                </button>
              )}
            </div>

            <div className="inline-flex bg-slate-950 p-1 rounded-xl border border-slate-800 shrink-0">
              <button
                type="button"
                onClick={() => setViewMode('cards')}
                className={`px-3 py-1.5 rounded-lg text-xs font-medium flex items-center space-x-1.5 transition ${
                  viewMode === 'cards'
                    ? 'bg-indigo-600 text-white'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                <LayoutGrid className="w-3.5 h-3.5" />
                <span>Карточки</span>
              </button>
              <button
                type="button"
                onClick={() => setViewMode('table')}
                className={`px-3 py-1.5 rounded-lg text-xs font-medium flex items-center space-x-1.5 transition ${
                  viewMode === 'table'
                    ? 'bg-indigo-600 text-white'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                <TableIcon className="w-3.5 h-3.5" />
                <span>Таблица</span>
              </button>
            </div>
          </div>
        </div>

        {/* Фильтр категорий */}
        <div className="flex flex-wrap items-center gap-2 pt-4 border-t border-slate-800/80">
          {CATEGORIES.map((category) => {
            const active = selectedCategory === category;
            return (
              <button
                key={category}
                type="button"
                onClick={() => setSelectedCategory(category)}
                className={`px-3 py-1.5 rounded-lg text-xs font-medium transition whitespace-nowrap ${
                  active
                    ? 'bg-indigo-600 text-white'
                    : 'bg-slate-950 text-slate-400 hover:text-slate-200 hover:bg-slate-800 border border-slate-800'
                }`}
              >
                <span>{category}</span>
                <span className={`ml-1.5 font-mono tabular-nums ${active ? 'text-indigo-200' : 'text-slate-500'}`}>
                  ({categoryCounts[category] ?? 0})
                </span>
              </button>
            );
          })}
        </div>
      </div>

      {/* Интерактивная структурная карта узлов */}
      <section className="bg-slate-900 border border-slate-800 rounded-2xl p-6 space-y-4">
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-2">
          <div>
            <h2 className="text-base font-semibold text-white">
              Структурная карта узлов схемы (01–16)
            </h2>
            <p className="text-xs text-slate-400">
              Нажмите на любой узел для быстрого перехода к описанию и позиционным обозначениям.
            </p>
          </div>
          <span className="text-xs font-mono text-slate-400 tabular-nums">
            Показано: {filteredNodes.length} из {SCHEMATIC_NODES.length}
          </span>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-5 gap-3">
          {STRUCTURAL_GROUPS.map((group) => (
            <div
              key={group.title}
              className="bg-slate-950/70 rounded-xl p-3.5 flex flex-col justify-between space-y-3"
            >
              <div>
                <div className="flex items-center space-x-2 mb-1">
                  {group.icon}
                  <h3 className="text-xs font-semibold text-white leading-snug">
                    {group.title}
                  </h3>
                </div>
                <p className="text-[11px] text-slate-500 leading-snug">{group.subtitle}</p>
              </div>

              <div className="space-y-1.5">
                {group.nodeIds.map((id) => {
                  const node = SCHEMATIC_NODES.find((n) => n.id === id);
                  if (!node) return null;
                  const isHighlighted = highlightedNodeId === node.id;
                  return (
                    <button
                      key={node.id}
                      type="button"
                      onClick={() => handleJumpToNode(node.id)}
                      className={`w-full text-left px-2.5 py-2 rounded-lg text-xs transition flex items-center justify-between group ${
                        isHighlighted
                          ? 'bg-indigo-600 text-white'
                          : 'bg-slate-900 hover:bg-slate-800 text-slate-300'
                      }`}
                    >
                      <span className="truncate pr-2">
                        <span
                          className={`font-mono font-bold tabular-nums mr-1.5 ${
                            isHighlighted ? 'text-indigo-200' : group.accentClass
                          }`}
                        >
                          {String(node.id).padStart(2, '0')}
                        </span>
                        <span className="truncate">{node.schematicLabel}</span>
                      </span>
                      <ArrowRight
                        className={`w-3.5 h-3.5 shrink-0 transition-transform group-hover:translate-x-0.5 ${
                          isHighlighted ? 'text-white' : 'text-slate-500'
                        }`}
                      />
                    </button>
                  );
                })}
              </div>
            </div>
          ))}
        </div>
      </section>

      {/* Пустой результат */}
      {filteredNodes.length === 0 ? (
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-10 text-center space-y-4">
          <p className="text-slate-300 text-sm">
            По запросу <span className="font-mono text-white">«{searchQuery}»</span> в выбранной категории узлы не найдены.
          </p>
          <button
            type="button"
            onClick={resetFilters}
            className="inline-flex items-center space-x-2 bg-indigo-600 hover:bg-indigo-500 text-white text-xs font-medium px-4 py-2.5 rounded-xl transition"
          >
            <RotateCcw className="w-4 h-4" />
            <span>Сбросить фильтры</span>
          </button>
        </div>
      ) : viewMode === 'cards' ? (
        /* Представление карточками */
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {filteredNodes.map((node: SchematicNode) => {
            const isHighlighted = highlightedNodeId === node.id;
            const quickPages = getQuickSheets(node.sheetPages);
            const hasMorePages = node.sheetPages.length > quickPages.length;

            return (
              <article
                key={node.id}
                id={`schematic-node-${node.id}`}
                className={`bg-slate-900 border rounded-2xl p-6 flex flex-col justify-between transition ${
                  isHighlighted
                    ? 'border-indigo-500 ring-1 ring-indigo-500/40'
                    : 'border-slate-800 hover:border-slate-700'
                }`}
              >
                <div className="space-y-4">
                  {/* Строка метаданных без декоративных пилюль */}
                  <div className="flex flex-wrap items-center justify-between gap-2 text-xs">
                    <div className="flex flex-wrap items-center gap-x-2 gap-y-1 font-mono">
                      <span className="text-indigo-400 font-bold tabular-nums">
                        {node.code.toUpperCase()}
                      </span>
                      <span className="text-slate-600">·</span>
                      <span className="text-slate-300 font-sans">{node.category}</span>
                      <span className="text-slate-600">·</span>
                      <span className="text-cyan-400">{node.schematicLabel}</span>
                    </div>
                  </div>

                  {/* Название и ссылки на листы */}
                  <div className="space-y-2">
                    <h2 className="text-xl font-bold text-white leading-snug">
                      <span className="font-mono text-indigo-400 tabular-nums mr-2">
                        {String(node.id).padStart(2, '0')}.
                      </span>
                      {node.title}
                    </h2>

                    <div className="flex flex-wrap items-center gap-2 text-xs text-slate-400">
                      <span>{node.sheetName}</span>
                      <div className="flex flex-wrap items-center gap-1.5">
                        {quickPages.map((page) => (
                          <button
                            key={page}
                            type="button"
                            onClick={() => onOpenSheet(page)}
                            className="inline-flex items-center space-x-1 bg-slate-800 hover:bg-indigo-600 text-slate-200 hover:text-white px-2.5 py-1 rounded-lg font-mono text-[11px] transition"
                            title={`Открыть лист ${page} PDF`}
                          >
                            <span>Открыть лист {page}</span>
                            <ExternalLink className="w-3 h-3" />
                          </button>
                        ))}
                        {hasMorePages && (
                          <span className="text-[11px] font-mono text-slate-500">
                            (+листы 15–23)
                          </span>
                        )}
                      </div>
                    </div>
                  </div>

                  {/* Описание работы узла */}
                  <p className="text-slate-300 text-sm leading-relaxed">
                    {node.description}
                  </p>

                  {node.notes && (
                    <div className="bg-amber-500/5 rounded-xl p-3 flex items-start space-x-2.5 text-xs text-amber-200/90">
                      <AlertCircle className="w-4 h-4 text-amber-400 shrink-0 mt-0.5" />
                      <span>{node.notes}</span>
                    </div>
                  )}

                  {/* Спецификации узла без вложенной внешней рамки */}
                  <div className="bg-slate-950/60 rounded-xl p-4 grid grid-cols-1 sm:grid-cols-2 gap-4 text-xs">
                    <div>
                      <span className="text-slate-500 uppercase tracking-wider font-semibold block mb-1.5">
                        Ключевые компоненты
                      </span>
                      <ul className="space-y-1 text-slate-200 font-mono">
                        {node.components.map((comp) => (
                          <li key={comp} className="leading-snug">
                            {comp}
                          </li>
                        ))}
                      </ul>
                    </div>

                    <div>
                      <span className="text-slate-500 uppercase tracking-wider font-semibold block mb-1.5">
                        Контрольные точки (TP)
                      </span>
                      <ul className="space-y-1 text-emerald-300 font-mono">
                        {node.testPoints.map((tp) => (
                          <li key={tp} className="leading-snug">
                            {tp}
                          </li>
                        ))}
                      </ul>
                    </div>

                    <div className="pt-3 border-t border-slate-800/80">
                      <span className="text-slate-500 uppercase tracking-wider font-semibold block mb-1">
                        Входы
                      </span>
                      <p className="text-slate-300 font-mono leading-relaxed">
                        {node.signalsIn.join(' · ')}
                      </p>
                    </div>

                    <div className="pt-3 border-t border-slate-800/80">
                      <span className="text-slate-500 uppercase tracking-wider font-semibold block mb-1">
                        Выходы
                      </span>
                      <p className="text-indigo-300 font-mono leading-relaxed">
                        {node.signalsOut.join(' · ')}
                      </p>
                    </div>
                  </div>
                </div>
              </article>
            );
          })}
        </div>
      ) : (
        /* Представление компактной таблицей */
        <div className="bg-slate-900 border border-slate-800 rounded-2xl overflow-hidden">
          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse">
              <thead>
                <tr className="bg-slate-950/80 border-b border-slate-800 text-xs text-slate-400 uppercase tracking-wider">
                  <th className="py-3.5 px-4 font-semibold w-20">Узел</th>
                  <th className="py-3.5 px-4 font-semibold">Наименование и блок схемы</th>
                  <th className="py-3.5 px-4 font-semibold">Компоненты</th>
                  <th className="py-3.5 px-4 font-semibold">Сигналы (Вход → Выход)</th>
                  <th className="py-3.5 px-4 font-semibold">Контрольные точки</th>
                  <th className="py-3.5 px-4 font-semibold">Листы</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-800/70 text-xs">
                {filteredNodes.map((node) => {
                  const quickPages = getQuickSheets(node.sheetPages);
                  return (
                    <tr
                      key={node.id}
                      id={`schematic-node-${node.id}`}
                      className={`hover:bg-slate-800/40 transition align-top ${
                        highlightedNodeId === node.id ? 'bg-indigo-950/40' : ''
                      }`}
                    >
                      <td className="py-4 px-4 font-mono font-bold text-indigo-400 tabular-nums whitespace-nowrap">
                        {String(node.id).padStart(2, '0')}
                      </td>
                      <td className="py-4 px-4 min-w-[220px]">
                        <div className="font-semibold text-white text-sm">{node.title}</div>
                        <div className="font-mono text-cyan-400 mt-0.5">{node.schematicLabel}</div>
                        <div className="text-slate-400 mt-1">{node.category}</div>
                      </td>
                      <td className="py-4 px-4 font-mono text-slate-300 min-w-[200px]">
                        {node.components.slice(0, 4).join(', ')}
                        {node.components.length > 4 && (
                          <span className="text-slate-500">
                            {' '}
                            (+{node.components.length - 4})
                          </span>
                        )}
                      </td>
                      <td className="py-4 px-4 font-mono text-slate-300 min-w-[210px]">
                        <div className="text-slate-400">Вх: {node.signalsIn.join(', ')}</div>
                        <div className="text-indigo-300 mt-1">Вых: {node.signalsOut.join(', ')}</div>
                      </td>
                      <td className="py-4 px-4 font-mono text-emerald-300 min-w-[180px]">
                        {node.testPoints.join(', ')}
                      </td>
                      <td className="py-4 px-4 whitespace-nowrap">
                        <div className="flex flex-wrap gap-1.5">
                          {quickPages.map((p) => (
                            <button
                              key={p}
                              type="button"
                              onClick={() => onOpenSheet(p)}
                              className="bg-slate-800 hover:bg-indigo-600 text-slate-200 hover:text-white font-mono px-2 py-1 rounded-md transition"
                            >
                              Л.{p}
                            </button>
                          ))}
                        </div>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}
