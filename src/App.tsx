import React, { useState } from 'react';
import {
  Cpu,
  FileText,
  Search,
  Zap,
  Info,
  BookOpen,
  Wifi,
  Volume2,
  CheckCircle2,
  Sparkles,
  Send,
  RefreshCw,
  ZoomIn,
  ZoomOut,
  Download,
  GitBranch,
  ExternalLink,
  ArrowRight,
} from 'lucide-react';
import { GoogleGenAI } from '@google/genai';
import NodesPage from './components/NodesPage';
import {
  PDF_URL,
  SHEETS,
  SCHEMATIC_NODES,
  DATASHEETS,
  NETS_LIST,
} from './data/schematicData';

type ActiveTab =
  | 'overview'
  | 'nodes'
  | 'sheets'
  | 'datasheets'
  | 'nets'
  | 'assistant';

export default function App() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('overview');
  const [selectedSheet, setSelectedSheet] = useState<number>(1);
  const [searchQuery, setSearchQuery] = useState<string>('');
  const [sheetZoom, setSheetZoom] = useState<number>(100);

  // Состояние ИИ-ассистента
  const [messages, setMessages] = useState<
    Array<{ role: 'user' | 'assistant'; text: string }>
  >([
    {
      role: 'assistant',
      text: 'Здравствуйте! Я ваш инженерный помощник по материнской плате домофонного коммутатора LeviSW-MB v1.2. В систему загружены все 16 нумерованных узлов схемы, 25 листов документации и спецификации 12 ключевых микросхем. Задавайте вопросы по схемотехнике, контрольным точкам TP1–TP201, шинам и питанию.',
    },
  ]);
  const [inputMessage, setInputMessage] = useState<string>('');
  const [isAiLoading, setIsAiLoading] = useState<boolean>(false);

  const handleOpenSheet = (pageNumber: number) => {
    setSelectedSheet(pageNumber);
    setActiveTab('sheets');
    window.scrollTo({ top: 0, behavior: 'smooth' });
  };

  const handleSendMessage = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!inputMessage.trim() || isAiLoading) return;

    const userText = inputMessage.trim();
    setInputMessage('');
    setMessages((prev) => [...prev, { role: 'user', text: userText }]);
    setIsAiLoading(true);

    try {
      const apiKey = import.meta.env.VITE_GEMINI_API_KEY || '';
      if (!apiKey) {
        setTimeout(() => {
          setMessages((prev) => [
            ...prev,
            {
              role: 'assistant',
              text: `[Автономный режим справочника]: По запросу «${userText}» доступны данные всех 16 нумерованных узлов (Узел 01..16), 25 листов КД и 12 типов ИС (ESP32-PICO-V3, TCA9535PWR, MAX13487EESA+, 74LVC2G17GW, ADG1608BR, MC33879APEKR2, TLE94112EL, TPS65261RHBR, INA226AIDGSR, MCP4532T-104E/MS, LM22675MRX-5.0, TL431ACDCKR). Для генерации развернутых ответов настройте переменную VITE_GEMINI_API_KEY.`,
            },
          ]);
          setIsAiLoading(false);
        }, 600);
        return;
      }

      const nodesSummary = SCHEMATIC_NODES.map(
        (n) =>
          `${n.code} (${n.schematicLabel}) — ${n.title}; листы ${n.sheetPages.join(',')}; ИС/элементы: ${n.components.join(', ')}; TP: ${n.testPoints.join(', ')}`
      ).join('\n');

      const dsSummary = DATASHEETS.map(
        (d) =>
          `${d.ref}: ${d.name} (${d.manufacturer}), питание ${d.voltage}, интерфейс ${d.interface}, листы ${d.sheetPages.join(',')}`
      ).join('\n');

      const ai = new GoogleGenAI({ apiKey });
      const response = await ai.models.generateContent({
        model: 'gemini-2.5-flash',
        contents: `Вы — главный инженер-схемотехник по плате домофонного коммутатора LeviSW-MB v1.2 (матрица 8×10 на 80 абонентов). Отвечайте строго на русском языке, опираясь только на проверенную документацию проекта (25 страниц PDF).

Нумерованные узлы схемы (01–16):
${nodesSummary}

Спецификации микросхем:
${dsSummary}

Вопрос пользователя: ${userText}`,
      });

      setMessages((prev) => [
        ...prev,
        { role: 'assistant', text: response.text || 'Ответ не был сгенерирован.' },
      ]);
    } catch (err: unknown) {
      const errorMessage = err instanceof Error ? err.message : String(err);
      setMessages((prev) => [
        ...prev,
        { role: 'assistant', text: `Ошибка обращения к ИИ: ${errorMessage}` },
      ]);
    } finally {
      setIsAiLoading(false);
    }
  };

  const filteredDatasheets = DATASHEETS.filter(
    (d) =>
      d.ref.toLowerCase().includes(searchQuery.toLowerCase()) ||
      d.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      d.manufacturer.toLowerCase().includes(searchQuery.toLowerCase()) ||
      d.description.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const filteredNets = NETS_LIST.filter(
    (n) =>
      n.net.toLowerCase().includes(searchQuery.toLowerCase()) ||
      n.description.toLowerCase().includes(searchQuery.toLowerCase()) ||
      n.category.toLowerCase().includes(searchQuery.toLowerCase()) ||
      n.sheets.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const currentSheet =
    SHEETS.find((s) => s.number === selectedSheet) || SHEETS[0];

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col font-sans">
      {/* 3-зонная верхняя панель навигации */}
      <header className="bg-slate-900 border-b border-slate-800 sticky top-0 z-50 px-4 lg:px-8 py-3 flex items-center justify-between gap-4">
        {/* Левая зона: однострочный логотип */}
        <button
          type="button"
          onClick={() => setActiveTab('overview')}
          className="flex items-center space-x-2.5 shrink-0 text-left group"
        >
          <div className="bg-indigo-600 p-2 rounded-xl text-white">
            <Cpu className="w-5 h-5" />
          </div>
          <span className="font-bold text-base lg:text-lg tracking-tight text-white whitespace-nowrap">
            LeviSW-MB v1.2
          </span>
        </button>

        {/* Центральная зона: навигация */}
        <nav className="hidden md:flex items-center space-x-1 bg-slate-950/70 p-1 rounded-xl border border-slate-800 overflow-x-auto">
          <button
            type="button"
            onClick={() => setActiveTab('overview')}
            className={`px-3 py-2 rounded-lg text-xs lg:text-sm font-medium transition whitespace-nowrap shrink-0 flex items-center space-x-1.5 ${
              activeTab === 'overview'
                ? 'bg-indigo-600 text-white'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'
            }`}
          >
            <Info className="w-4 h-4" />
            <span>Обзор</span>
          </button>
          <button
            type="button"
            onClick={() => setActiveTab('nodes')}
            className={`px-3 py-2 rounded-lg text-xs lg:text-sm font-medium transition whitespace-nowrap shrink-0 flex items-center space-x-1.5 ${
              activeTab === 'nodes'
                ? 'bg-indigo-600 text-white'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'
            }`}
          >
            <GitBranch className="w-4 h-4" />
            <span>Узлы схемы ({SCHEMATIC_NODES.length})</span>
          </button>
          <button
            type="button"
            onClick={() => setActiveTab('sheets')}
            className={`px-3 py-2 rounded-lg text-xs lg:text-sm font-medium transition whitespace-nowrap shrink-0 flex items-center space-x-1.5 ${
              activeTab === 'sheets'
                ? 'bg-indigo-600 text-white'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'
            }`}
          >
            <FileText className="w-4 h-4" />
            <span>Схемы ({SHEETS.length})</span>
          </button>
          <button
            type="button"
            onClick={() => setActiveTab('datasheets')}
            className={`px-3 py-2 rounded-lg text-xs lg:text-sm font-medium transition whitespace-nowrap shrink-0 flex items-center space-x-1.5 ${
              activeTab === 'datasheets'
                ? 'bg-indigo-600 text-white'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'
            }`}
          >
            <BookOpen className="w-4 h-4" />
            <span>Даташиты ИС</span>
          </button>
          <button
            type="button"
            onClick={() => setActiveTab('nets')}
            className={`px-3 py-2 rounded-lg text-xs lg:text-sm font-medium transition whitespace-nowrap shrink-0 flex items-center space-x-1.5 ${
              activeTab === 'nets'
                ? 'bg-indigo-600 text-white'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'
            }`}
          >
            <Zap className="w-4 h-4" />
            <span>Цепи</span>
          </button>
          <button
            type="button"
            onClick={() => setActiveTab('assistant')}
            className={`px-3 py-2 rounded-lg text-xs lg:text-sm font-medium transition whitespace-nowrap shrink-0 flex items-center space-x-1.5 ${
              activeTab === 'assistant'
                ? 'bg-indigo-600 text-white'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'
            }`}
          >
            <Sparkles className="w-4 h-4 text-amber-400" />
            <span>ИИ-Инженер</span>
          </button>
        </nav>

        {/* Правая зона: главное действие */}
        <a
          href={PDF_URL}
          download
          target="_blank"
          rel="noreferrer"
          className="bg-slate-800 hover:bg-slate-700 text-slate-100 border border-slate-700 px-3.5 py-2 rounded-xl text-xs font-medium transition flex items-center space-x-2 whitespace-nowrap shrink-0"
        >
          <Download className="w-4 h-4 text-indigo-400" />
          <span className="hidden sm:inline">Скачать КД (PDF)</span>
          <span className="sm:hidden">PDF</span>
        </a>
      </header>

      {/* Мобильная навигация */}
      <div className="md:hidden flex items-center gap-1.5 bg-slate-900 border-b border-slate-800 px-3 py-2 overflow-x-auto">
        <button
          type="button"
          onClick={() => setActiveTab('overview')}
          className={`px-3 py-1.5 rounded-lg text-xs font-medium whitespace-nowrap shrink-0 ${
            activeTab === 'overview' ? 'bg-indigo-600 text-white' : 'text-slate-400'
          }`}
        >
          Обзор
        </button>
        <button
          type="button"
          onClick={() => setActiveTab('nodes')}
          className={`px-3 py-1.5 rounded-lg text-xs font-medium whitespace-nowrap shrink-0 ${
            activeTab === 'nodes' ? 'bg-indigo-600 text-white' : 'text-slate-400'
          }`}
        >
          Узлы ({SCHEMATIC_NODES.length})
        </button>
        <button
          type="button"
          onClick={() => setActiveTab('sheets')}
          className={`px-3 py-1.5 rounded-lg text-xs font-medium whitespace-nowrap shrink-0 ${
            activeTab === 'sheets' ? 'bg-indigo-600 text-white' : 'text-slate-400'
          }`}
        >
          Схемы ({SHEETS.length})
        </button>
        <button
          type="button"
          onClick={() => setActiveTab('datasheets')}
          className={`px-3 py-1.5 rounded-lg text-xs font-medium whitespace-nowrap shrink-0 ${
            activeTab === 'datasheets' ? 'bg-indigo-600 text-white' : 'text-slate-400'
          }`}
        >
          Даташиты
        </button>
        <button
          type="button"
          onClick={() => setActiveTab('nets')}
          className={`px-3 py-1.5 rounded-lg text-xs font-medium whitespace-nowrap shrink-0 ${
            activeTab === 'nets' ? 'bg-indigo-600 text-white' : 'text-slate-400'
          }`}
        >
          Цепи
        </button>
        <button
          type="button"
          onClick={() => setActiveTab('assistant')}
          className={`px-3 py-1.5 rounded-lg text-xs font-medium whitespace-nowrap shrink-0 ${
            activeTab === 'assistant' ? 'bg-indigo-600 text-white' : 'text-slate-400'
          }`}
        >
          ИИ-Инженер
        </button>
      </div>

      {/* Основное содержимое */}
      <main className="flex-1 p-4 lg:p-8 max-w-7xl mx-auto w-full">
        {activeTab === 'overview' && (
          <div className="space-y-8">
            {/* Главный блок обзора */}
            <div className="bg-gradient-to-br from-indigo-950/60 via-slate-900 to-slate-900 border border-indigo-500/20 rounded-3xl p-6 lg:p-10 relative overflow-hidden">
              <div className="max-w-3xl space-y-5">
                <div className="flex flex-wrap items-center gap-x-3 gap-y-1 text-xs font-mono text-emerald-400">
                  <span className="inline-flex items-center space-x-1.5">
                    <CheckCircle2 className="w-3.5 h-3.5" />
                    <span>КД LeviSW-MB_v1.2E2[A]</span>
                  </span>
                  <span className="text-slate-600">·</span>
                  <span className="text-slate-300 tabular-nums">25 листов схем и ПП</span>
                  <span className="text-slate-600">·</span>
                  <span className="text-slate-300 tabular-nums">16 узлов · TP1–TP201</span>
                </div>

                <h1 className="text-3xl lg:text-5xl font-extrabold text-white tracking-tight">
                  Материнская плата LeviSW-MB v1.2
                </h1>

                <p className="text-slate-300 text-base lg:text-lg leading-relaxed">
                  Материнская плата домофонного коммутатора на 80 абонентов с координатно-матричной схемой подключения (8×10). Питание 12–24В, детекция состояния абонентских трубок LU1/LU2, защита от «перевертышей», поддержка платы речевого оповещения Левитан и сервисный интерфейс Wi-Fi / BLE / RS-485.
                </p>

                <div className="flex flex-wrap gap-3 pt-1">
                  <button
                    type="button"
                    onClick={() => setActiveTab('nodes')}
                    className="bg-indigo-600 hover:bg-indigo-500 text-white font-medium px-6 py-3 rounded-xl transition flex items-center space-x-2"
                  >
                    <GitBranch className="w-5 h-5" />
                    <span>Узлы схемы (01–16)</span>
                  </button>
                  <button
                    type="button"
                    onClick={() => setActiveTab('sheets')}
                    className="bg-slate-800 hover:bg-slate-700 text-slate-200 font-medium px-6 py-3 rounded-xl transition border border-slate-700 flex items-center space-x-2"
                  >
                    <FileText className="w-5 h-5" />
                    <span>Просмотр схем ({SHEETS.length})</span>
                  </button>
                  <button
                    type="button"
                    onClick={() => setActiveTab('datasheets')}
                    className="bg-slate-800 hover:bg-slate-700 text-slate-200 font-medium px-6 py-3 rounded-xl transition border border-slate-700 flex items-center space-x-2"
                  >
                    <BookOpen className="w-5 h-5" />
                    <span>Даташиты ИС ({DATASHEETS.length})</span>
                  </button>
                </div>
              </div>
            </div>

            {/* Ключевые подсистемы */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
              <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl">
                <div className="w-10 h-10 bg-indigo-600/20 text-indigo-400 rounded-xl flex items-center justify-center mb-4">
                  <Cpu className="w-5 h-5" />
                </div>
                <h2 className="text-lg font-bold text-white mb-1">Матрица 8×10</h2>
                <p className="text-slate-400 text-sm leading-relaxed">
                  80 абонентов: десятки D0–D7 на ADG1608BR (DA1) + MC33879 (D1), единицы E0–E9 на TLE94112EL (D2).
                </p>
              </div>

              <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl">
                <div className="w-10 h-10 bg-emerald-600/20 text-emerald-400 rounded-xl flex items-center justify-center mb-4">
                  <Zap className="w-5 h-5" />
                </div>
                <h2 className="text-lg font-bold text-white mb-1">Питание и телеметрия</h2>
                <p className="text-slate-400 text-sm leading-relaxed">
                  Вход 12–24В, TPS65261 (D3), два LM22675 (DP1/DP2), узел L1ADJ (TL431) и контроль на MCP4532 + INA226.
                </p>
              </div>

              <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl">
                <div className="w-10 h-10 bg-cyan-600/20 text-cyan-400 rounded-xl flex items-center justify-center mb-4">
                  <Wifi className="w-5 h-5" />
                </div>
                <h2 className="text-lg font-bold text-white mb-1">Ядро МК и связь</h2>
                <p className="text-slate-400 text-sm leading-relaxed">
                  ESP32-PICO-V3 (DD4) с антенной 2.4 ГГц, расширитель TCA9535 (DD3), RS-485 MAX13487 (DD2), 1-Wire и LU1/LU2 (DD1).
                </p>
              </div>

              <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl">
                <div className="w-10 h-10 bg-amber-600/20 text-amber-400 rounded-xl flex items-center justify-center mb-4">
                  <Volume2 className="w-5 h-5" />
                </div>
                <h2 className="text-lg font-bold text-white mb-1">Оповещение Левитан</h2>
                <p className="text-slate-400 text-sm leading-relaxed">
                  Разъемы XS2/XS3, коммутируемое питание +Vamp (VT4/VT5), каналы звука DAC1/DAC2 и матрица AMP1..AMP8.
                </p>
              </div>
            </div>

            {/* Быстрый обзор нумерованных узлов */}
            <section className="bg-slate-900 border border-slate-800 rounded-2xl p-6 space-y-4">
              <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-2">
                <div>
                  <h2 className="text-lg font-bold text-white">
                    Нумерованная структура схемы (Узлы 01–16)
                  </h2>
                  <p className="text-xs text-slate-400">
                    Все функциональные блоки принципиальной схемы и чертежей платы с привязкой к листам PDF.
                  </p>
                </div>
                <button
                  type="button"
                  onClick={() => setActiveTab('nodes')}
                  className="inline-flex items-center space-x-1.5 text-xs font-medium text-indigo-400 hover:text-indigo-300 transition shrink-0"
                >
                  <span>Открыть страницу узлов</span>
                  <ArrowRight className="w-4 h-4" />
                </button>
              </div>

              <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3">
                {SCHEMATIC_NODES.map((node) => (
                  <button
                    key={node.id}
                    type="button"
                    onClick={() => setActiveTab('nodes')}
                    className="bg-slate-950/70 hover:bg-slate-800/80 rounded-xl p-3.5 text-left transition group"
                  >
                    <div className="flex items-center justify-between text-xs font-mono mb-1">
                      <span className="text-indigo-400 font-bold tabular-nums">
                        {node.code}
                      </span>
                      <span className="text-slate-500">
                        Л.{node.sheetPages[0]}
                        {node.sheetPages.length > 1 ? '+' : ''}
                      </span>
                    </div>
                    <div className="text-sm font-medium text-slate-200 group-hover:text-white leading-snug">
                      {node.title}
                    </div>
                    <div className="text-[11px] font-mono text-slate-500 mt-1 truncate">
                      {node.schematicLabel}
                    </div>
                  </button>
                ))}
              </div>
            </section>
          </div>
        )}

        {activeTab === 'nodes' && <NodesPage onOpenSheet={handleOpenSheet} />}

        {activeTab === 'sheets' && (
          <div className="space-y-6">
            <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
              <div>
                <h1 className="text-xl font-bold text-white flex items-center space-x-2">
                  <FileText className="w-5 h-5 text-indigo-400" />
                  <span>Электрические схемы и чертежи печатной платы</span>
                </h1>
                <p className="text-slate-400 text-sm mt-1">
                  Полный комплект конструкторской документации (25 листов).
                </p>
              </div>

              <div className="w-full md:w-auto">
                <select
                  value={selectedSheet}
                  onChange={(e) => setSelectedSheet(Number(e.target.value))}
                  className="bg-slate-950 border border-slate-700 text-slate-200 px-4 py-2.5 rounded-xl text-sm font-medium w-full md:w-96 focus:outline-none focus:border-indigo-500"
                >
                  {SHEETS.map((sheet) => (
                    <option key={sheet.number} value={sheet.number}>
                      Стр. {sheet.number}: {sheet.name} ({sheet.docNumber})
                    </option>
                  ))}
                </select>
              </div>
            </div>

            <div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
              <div className="lg:col-span-1 bg-slate-900 border border-slate-800 rounded-2xl p-4 max-h-[720px] overflow-y-auto space-y-1">
                <h2 className="text-xs font-semibold text-slate-400 uppercase tracking-wider px-3 py-2">
                  Листы ({SHEETS.length})
                </h2>
                {SHEETS.map((s) => (
                  <button
                    key={s.number}
                    type="button"
                    onClick={() => setSelectedSheet(s.number)}
                    className={`w-full text-left px-3 py-2.5 rounded-xl text-xs font-medium transition flex items-center justify-between ${
                      selectedSheet === s.number
                        ? 'bg-indigo-600 text-white font-semibold'
                        : 'text-slate-300 hover:bg-slate-800/60'
                    }`}
                  >
                    <div className="truncate pr-2">
                      <span className="opacity-75 font-mono tabular-nums mr-2">
                        #{String(s.number).padStart(2, '0')}
                      </span>
                      <span>{s.name}</span>
                    </div>
                    <span
                      className={`text-[10px] shrink-0 ${
                        selectedSheet === s.number ? 'text-indigo-100' : 'text-slate-500'
                      }`}
                    >
                      {s.category}
                    </span>
                  </button>
                ))}
              </div>

              <div className="lg:col-span-3 bg-slate-900 border border-slate-800 rounded-2xl p-6 flex flex-col">
                <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 mb-4 pb-4 border-b border-slate-800">
                  <div>
                    <div className="flex flex-wrap items-center gap-x-2 gap-y-1 text-xs font-mono">
                      <span className="text-indigo-400 font-semibold tabular-nums">
                        Стр. {currentSheet.number} из {SHEETS.length}
                      </span>
                      <span className="text-slate-600">·</span>
                      <span className="text-slate-300">{currentSheet.docNumber}</span>
                      <span className="text-slate-600">·</span>
                      <span className="text-slate-400">{currentSheet.filename}</span>
                      <span className="text-slate-600">·</span>
                      <span className="text-emerald-400 font-sans">
                        {currentSheet.category}
                      </span>
                    </div>
                    <h2 className="text-xl font-bold text-white mt-1">
                      {currentSheet.name}
                    </h2>
                    <p className="text-slate-400 text-xs mt-1">
                      {currentSheet.description}
                    </p>
                  </div>

                  <div className="flex items-center space-x-2 shrink-0">
                    <button
                      type="button"
                      onClick={() => setSheetZoom((prev) => Math.max(50, prev - 25))}
                      className="p-2 bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-lg border border-slate-700 transition"
                      title="Уменьшить"
                    >
                      <ZoomOut className="w-4 h-4" />
                    </button>
                    <span className="text-xs font-mono tabular-nums text-slate-300 px-2">
                      {sheetZoom}%
                    </span>
                    <button
                      type="button"
                      onClick={() => setSheetZoom((prev) => Math.min(200, prev + 25))}
                      className="p-2 bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-lg border border-slate-700 transition"
                      title="Увеличить"
                    >
                      <ZoomIn className="w-4 h-4" />
                    </button>
                    <a
                      href={`${PDF_URL}#page=${currentSheet.number}`}
                      target="_blank"
                      rel="noreferrer"
                      className="p-2 bg-indigo-600 hover:bg-indigo-500 text-white rounded-lg transition flex items-center space-x-1.5 text-xs font-medium ml-1"
                      title="Открыть страницу PDF в новой вкладке"
                    >
                      <ExternalLink className="w-4 h-4" />
                      <span className="hidden sm:inline">Открыть PDF</span>
                    </a>
                  </div>
                </div>

                <div className="flex-1 bg-slate-950 rounded-xl overflow-hidden min-h-[580px]">
                  <iframe
                    key={`${currentSheet.number}-${sheetZoom}`}
                    src={`${PDF_URL}#page=${currentSheet.number}&zoom=${sheetZoom}`}
                    className="w-full h-[620px] border-0 bg-white"
                    title={`Лист ${currentSheet.number} — ${currentSheet.name}`}
                  />
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'datasheets' && (
          <div className="space-y-6">
            <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
              <div>
                <h1 className="text-xl font-bold text-white flex items-center space-x-2">
                  <BookOpen className="w-5 h-5 text-indigo-400" />
                  <span>Даташиты и технические спецификации микросхем</span>
                </h1>
                <p className="text-slate-400 text-sm mt-1">
                  Характеристики, корпуса и привязка к листам схемы для всех 12 семейств ИС платы LeviSW-MB v1.2.
                </p>
              </div>

              <div className="relative w-full md:w-80">
                <Search className="w-4 h-4 text-slate-400 absolute left-3.5 top-3" />
                <input
                  type="text"
                  placeholder="Поиск по ИС, корпусу, производителю..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="w-full bg-slate-950 border border-slate-700 text-slate-200 pl-10 pr-4 py-2.5 rounded-xl text-sm focus:outline-none focus:border-indigo-500"
                />
              </div>
            </div>

            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              {filteredDatasheets.map((ds) => (
                <div
                  key={`${ds.ref}-${ds.name}`}
                  className="bg-slate-900 border border-slate-800 p-6 rounded-2xl flex flex-col justify-between hover:border-slate-700 transition"
                >
                  <div>
                    <div className="flex flex-wrap items-center justify-between gap-2 text-xs mb-2">
                      <span className="font-mono font-bold text-indigo-400">
                        {ds.ref}
                      </span>
                      <span className="text-slate-400">{ds.manufacturer}</span>
                    </div>

                    <h2 className="text-xl font-bold text-white mb-2">{ds.name}</h2>
                    <p className="text-slate-300 text-xs leading-relaxed mb-4">
                      {ds.description}
                    </p>

                    <div className="grid grid-cols-2 gap-3 mb-4 text-xs bg-slate-950/60 p-3.5 rounded-xl">
                      <div>
                        <span className="text-slate-500 block">Корпус</span>
                        <strong className="text-slate-200 font-mono">{ds.package}</strong>
                      </div>
                      <div>
                        <span className="text-slate-500 block">Питание</span>
                        <strong className="text-slate-200 font-mono">{ds.voltage}</strong>
                      </div>
                      <div className="col-span-2 pt-2 border-t border-slate-800/80">
                        <span className="text-slate-500 block">Интерфейс</span>
                        <strong className="text-indigo-300 font-mono">{ds.interface}</strong>
                      </div>
                    </div>

                    <div className="space-y-1.5">
                      <span className="text-xs font-semibold text-slate-400 uppercase tracking-wider">
                        Ключевые особенности
                      </span>
                      {ds.keyFeatures.map((feat) => (
                        <div
                          key={feat}
                          className="flex items-start space-x-2 text-xs text-slate-300"
                        >
                          <CheckCircle2 className="w-3.5 h-3.5 text-emerald-400 shrink-0 mt-0.5" />
                          <span>{feat}</span>
                        </div>
                      ))}
                    </div>
                  </div>

                  <div className="mt-5 pt-4 border-t border-slate-800 flex flex-wrap items-center justify-between gap-2 text-xs">
                    <span className="text-emerald-400 font-medium flex items-center space-x-1.5">
                      <CheckCircle2 className="w-4 h-4" />
                      <span>Спецификация в базе</span>
                    </span>
                    <div className="flex flex-wrap items-center gap-1.5">
                      {ds.sheetPages.map((page) => (
                        <button
                          key={page}
                          type="button"
                          onClick={() => handleOpenSheet(page)}
                          className="bg-slate-800 hover:bg-indigo-600 text-slate-200 hover:text-white px-2.5 py-1 rounded-lg font-mono text-[11px] transition inline-flex items-center space-x-1"
                        >
                          <span>Лист {page}</span>
                          <ExternalLink className="w-3 h-3" />
                        </button>
                      ))}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'nets' && (
          <div className="space-y-6">
            <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
              <div>
                <h1 className="text-xl font-bold text-white flex items-center space-x-2">
                  <Zap className="w-5 h-5 text-indigo-400" />
                  <span>Каталог электрических цепей и сигналов</span>
                </h1>
                <p className="text-slate-400 text-sm mt-1">
                  Шины питания, интерфейсы связи, матричные линии D0–D7 / E0–E9 и магистраль управления.
                </p>
              </div>

              <div className="relative w-full md:w-80">
                <Search className="w-4 h-4 text-slate-400 absolute left-3.5 top-3" />
                <input
                  type="text"
                  placeholder="Поиск по цепи, категории, листу..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="w-full bg-slate-950 border border-slate-700 text-slate-200 pl-10 pr-4 py-2.5 rounded-xl text-sm focus:outline-none focus:border-indigo-500"
                />
              </div>
            </div>

            <div className="bg-slate-900 border border-slate-800 rounded-2xl overflow-hidden">
              <div className="overflow-x-auto">
                <table className="w-full text-left border-collapse">
                  <thead>
                    <tr className="bg-slate-950/80 border-b border-slate-800 text-xs text-slate-400 uppercase tracking-wider">
                      <th className="py-4 px-6 font-semibold">Имя цепи / шины</th>
                      <th className="py-4 px-6 font-semibold">Назначение и описание</th>
                      <th className="py-4 px-6 font-semibold">Категория</th>
                      <th className="py-4 px-6 font-semibold">Листы</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-800/60 text-sm">
                    {filteredNets.map((n) => (
                      <tr key={n.net} className="hover:bg-slate-800/40 transition">
                        <td className="py-4 px-6 font-mono font-bold text-indigo-400 whitespace-nowrap">
                          {n.net}
                        </td>
                        <td className="py-4 px-6 text-slate-300">{n.description}</td>
                        <td className="py-4 px-6 text-xs font-medium text-slate-300 whitespace-nowrap">
                          {n.category}
                        </td>
                        <td className="py-4 px-6 text-xs font-mono text-slate-400 whitespace-nowrap">
                          {n.sheets}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'assistant' && (
          <div className="space-y-6 max-w-4xl mx-auto">
            <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl flex items-center space-x-4">
              <div className="w-12 h-12 bg-indigo-600/20 text-indigo-400 rounded-2xl flex items-center justify-center shrink-0">
                <Sparkles className="w-6 h-6 text-amber-400" />
              </div>
              <div>
                <h1 className="text-xl font-bold text-white">
                  ИИ-Инженер по схемотехнике LeviSW-MB v1.2
                </h1>
                <p className="text-slate-400 text-sm">
                  База из 16 узлов схемы, 25 листов КД и 12 семейств микросхем загружена. Задавайте технические вопросы.
                </p>
              </div>
            </div>

            <div className="bg-slate-900 border border-slate-800 rounded-2xl flex flex-col h-[520px]">
              <div className="flex-1 p-6 overflow-y-auto space-y-4">
                {messages.map((msg, index) => (
                  <div
                    key={index}
                    className={`flex ${
                      msg.role === 'user' ? 'justify-end' : 'justify-start'
                    }`}
                  >
                    <div
                      className={`max-w-[85%] rounded-2xl px-5 py-3.5 text-sm leading-relaxed whitespace-pre-wrap ${
                        msg.role === 'user'
                          ? 'bg-indigo-600 text-white'
                          : 'bg-slate-950 text-slate-200'
                      }`}
                    >
                      {msg.text}
                    </div>
                  </div>
                ))}
                {isAiLoading && (
                  <div className="flex justify-start">
                    <div className="bg-slate-950 text-slate-400 rounded-2xl px-5 py-3.5 text-sm flex items-center space-x-2">
                      <RefreshCw className="w-4 h-4 animate-spin text-indigo-400" />
                      <span>Анализ базы схемы и даташитов...</span>
                    </div>
                  </div>
                )}
              </div>

              <form
                onSubmit={handleSendMessage}
                className="p-4 bg-slate-950/80 border-t border-slate-800 flex items-center space-x-3"
              >
                <input
                  type="text"
                  placeholder="Например: за что отвечает Узел 10 VOffset или какие TP у RS-485?"
                  value={inputMessage}
                  onChange={(e) => setInputMessage(e.target.value)}
                  className="flex-1 bg-slate-900 border border-slate-700 text-slate-200 px-4 py-3 rounded-xl text-sm focus:outline-none focus:border-indigo-500"
                />
                <button
                  type="submit"
                  disabled={isAiLoading || !inputMessage.trim()}
                  className="bg-indigo-600 hover:bg-indigo-500 disabled:opacity-50 text-white px-5 py-3 rounded-xl font-medium transition flex items-center space-x-2 shrink-0"
                >
                  <Send className="w-4 h-4" />
                  <span className="hidden sm:inline">Отправить</span>
                </button>
              </form>
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
