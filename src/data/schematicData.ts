export interface SheetInfo {
  number: number;
  docNumber: string;
  name: string;
  filename: string;
  description: string;
  category: string;
}

export type NodeCategory =
  | 'Интерфейсы и связь'
  | 'Ядро МК'
  | 'Матричная коммутация'
  | 'Питание и телеметрия'
  | 'Конструкция ПП';

export interface SchematicNode {
  id: number;
  code: string;
  title: string;
  schematicLabel: string;
  category: NodeCategory;
  sheetPages: number[];
  sheetName: string;
  components: string[];
  signalsIn: string[];
  signalsOut: string[];
  testPoints: string[];
  description: string;
  notes?: string;
}

export interface DatasheetRecord {
  ref: string;
  name: string;
  manufacturer: string;
  package: string;
  voltage: string;
  interface: string;
  sheetPages: number[];
  description: string;
  keyFeatures: string[];
}

export interface NetRecord {
  net: string;
  category: 'Питание' | 'Связь' | 'Матрица' | 'Управление';
  sheets: string;
  description: string;
}

export const PDF_URL = '/assets/aistudio/LeviSW-MB_v1.2E2_A_____efc1b6d2.PDF';

export const SHEETS: SheetInfo[] = [
  { number: 1, docNumber: "Лист 1 из 13", name: "Содержание проекта", filename: "Table of content.SchDoc", description: "Титульный лист, краткое описание платы (80 абонентов), история версий (LeviCOM-MB v1 → LeviSW-MB_v1.2).", category: "Система" },
  { number: 2, docNumber: "Лист 2 из 13", name: "Основная схема коммутатора", filename: "Switcher.SchDoc", description: "Верхний иерархический уровень: клеммы XP1–XP3/XS1–XS5, плата Левитан, индикация LED1, кнопка SW1 и связи между модулями.", category: "Ядро" },
  { number: 3, docNumber: "Лист 3 из 13", name: "Интерфейсы 1-Wire, RS-485 и LU", filename: "Com-485-LU-1W.SchDoc", description: "Трансивер RS-485 (DD2 MAX13487), двойной триггер Шмитта для детекции линий трубок LU1/LU2 (DD1) и каскад 1-Wire / OpenDoor.", category: "Связь" },
  { number: 4, docNumber: "Лист 4 из 13", name: "Микроконтроллер ESP32 и периферия", filename: "MCU-ESP.SchDoc", description: "ESP32-PICO-V3 (DD4), RF-цепь с ANT1, I2C-расширитель TCA9535 (DD3), селекторы маршрутизации UART/1W, АЦП VAMPOK/VersionPCB, JTAG и Prog.", category: "МК" },
  { number: 5, docNumber: "Лист 5 из 13", name: "Коммутатор десятков D8 и VOffset", filename: "D8.SchDoc", description: "Аналоговый мультиплексор ADG1608BR (DA1) линий D0–D7 и 8-канальный драйвер защиты от «перевертышей» MC33879APEKR2 (D1).", category: "Коммутация" },
  { number: 6, docNumber: "Лист 6 из 13", name: "Коммутатор единиц E10", filename: "E10.SchDoc", description: "12-канальный полумостовой SPI-драйвер TLE94112EL (D2) для линий E0–E9, диодный мост VD24 и LC-фильтр возврата -LINE.", category: "Коммутация" },
  { number: 7, docNumber: "Лист 7 из 13", name: "Подсистема питания и L1ADJ", filename: "Power.SchDoc", description: "Входная защита POWERIN, ключ +Vamp (VT4/VT5), тройной DC-DC TPS65261RHBR (D3) и узел коррекции L1ADJ на TL431 (DV1).", category: "Питание" },
  { number: 8, docNumber: "Лист 8 из 13", name: "Канал управления MV1 (+V1)", filename: "ManageChannel.SchDoc", description: "Цифровой потенциометр MCP4532 (DA2) в цепи обратной связи FB1 и монитор тока/напряжения INA226 (DD5) для шины +V1.", category: "Мониторинг" },
  { number: 9, docNumber: "Лист 9 из 13", name: "Канал управления MV1e (Uload)", filename: "ManageChannel.SchDoc", description: "Управление порогом Uload на MCP4532 (DA4) и шунт R73 (микросхема INA226 DD7 на плате не устанавливается).", category: "Мониторинг" },
  { number: 10, docNumber: "Лист 9.1 из 13", name: "Преобразователь DC-DC L1", filename: "DC-DC.SchDoc", description: "Импульсный понижающий регулятор LM22675MRX-5.0 (DP1) для формирования шины +VCO_L1 из Uload.", category: "Питание" },
  { number: 11, docNumber: "Лист 9.2 из 13", name: "Преобразователь DC-DC V5", filename: "DC-DC.SchDoc", description: "Импульсный понижающий регулятор LM22675MRX-5.0 (DP2) для формирования стабильной логической шины +5VL из +Vdc.", category: "Питание" },
  { number: 12, docNumber: "Лист 10 из 13", name: "Канал управления MV2 (+V2)", filename: "ManageChannel.SchDoc", description: "Цифровой потенциометр MCP4532 (DA3) в цепи FB2 и телеметрия INA226 (DD6) для регулируемого канала +V2.", category: "Мониторинг" },
  { number: 13, docNumber: "Лист 11 из 13", name: "Механика и маркировка", filename: "Mechanical.SchDoc", description: "Крепежные отверстия, реперные знаки и текстовые слои маркировки платы LeviSW-MB_v1.2.", category: "Механика" },
  { number: 14, docNumber: "Сборка 1 из 3", name: "Сборочный чертеж (Top/Bottom)", filename: "LeviCom-MB.PCBDwf", description: "Вид сверху и снизу в масштабе 1:1 с позиционными обозначениями и технологическими примечаниями по монтажу.", category: "Плата" },
  { number: 15, docNumber: "Сборка 2 из 3", name: "3D-визуализация сборки", filename: "LeviCom-MB.PCBDwf", description: "Реалистичные ортогональные и изометрические 3D-виды верхней и нижней сторон печатной платы.", category: "Плата" },
  { number: 16, docNumber: "Сборка 3 из 3", name: "Изометрия корпуса и разъемов", filename: "LeviCom-MB.PCBDwf", description: "Векторные изометрические чертежи компоновки клеммников и высоких элементов (масштаб 1:1).", category: "Плата" },
  { number: 17, docNumber: "Стек 1 из 2", name: "Стек слоев печатной платы", filename: "PCBStack.PCBDwf", description: "4-слойная структура FR4 (общая толщина 1.587 мм): Top, GND, Power, Bottom, медь 1 oz (0.035 мм).", category: "Плата" },
  { number: 18, docNumber: "Стек 2 из 2", name: "Таблица механических и AUX-слоев", filename: "PCBStack.PCBDwf", description: "Спецификация Gerber-слоев профиля платы, V-Cut, клеевых точек, золочения и защитного покрытия.", category: "Плата" },
  { number: 19, docNumber: "Топология 1", name: "Разводка верхнего слоя (Top)", filename: "PCBLayout", description: "Медный рисунок проводников, полигонов и контактных площадок верхнего сигнального слоя (GTL).", category: "Плата" },
  { number: 20, docNumber: "Топология 2", name: "Разводка нижнего слоя (Bottom)", filename: "PCBLayout", description: "Медный рисунок проводников и полигонов нижнего слоя печатной платы (GBL).", category: "Плата" },
  { number: 21, docNumber: "Шелкография 1", name: "Монтажная схема верха (Top Overlay)", filename: "PCBSilk", description: "Позиционные обозначения, подписи клеммников (INTERCOM, E10 OUT, D8 OUT) и контрольных точек сверху.", category: "Плата" },
  { number: 22, docNumber: "Шелкография 2", name: "Монтажная схема низа (Bottom Overlay)", filename: "PCBSilk", description: "Размещение и маркировка компонентов на обратной стороне платы (VD7–VD14, RN24–RN25, L7–L8, DP1–DP2).", category: "Плата" },
  { number: 23, docNumber: "Топология 3", name: "Композитный вид цепей верхнего слоя", filename: "PCBRender", description: "Сводный чертеж трассировки с наложенными подписями электрических цепей на контактных площадках.", category: "Плата" },
  { number: 24, docNumber: "Стек (дубль) 1", name: "Стек слоев платы (приложение)", filename: "PCBStack.PCBDwf", description: "Дублирующая спецификация 4-слойного стека FR4 1.587 мм в конце комплекта КД.", category: "Плата" },
  { number: 25, docNumber: "Стек (дубль) 2", name: "Таблица слоев (приложение)", filename: "PCBStack.PCBDwf", description: "Дублирующая таблица механических (GM1–GM4) и вспомогательных слоев (GM9–GM22).", category: "Плата" },
];

export const SCHEMATIC_NODES: SchematicNode[] = [
  {
    id: 1,
    code: "Узел 01",
    title: "Входные/выходные клеммы и локальная индикация",
    schematicLabel: "INTERCOM / D8 OUT / E10 OUT / LED1 / SW1",
    category: "Интерфейсы и связь",
    sheetPages: [2],
    sheetName: "Switcher.SchDoc (Лист 2)",
    components: ["XP1 / XS1 (INTERCOM, 9-pin)", "XP2 / XS4 (D8 OUT, 8-pin)", "XP3 / XS5 (E10 OUT, 10-pin)", "LED1 (WS2812C)", "SW1 (MODE)", "R1 (0 Ом SW-OpenDoor)"],
    signalsIn: ["+VIN / -VIN (12–24В)", "A / B (RS-485)", "LU1 / LU2", "+Line / -Line", "Open/1W"],
    signalsOut: ["D0..D7 (десятки)", "E0..E9 (единицы)", "BUTTON", "LED"],
    testPoints: ["TP1..TP9 (INTERCOM)", "TP17, TP25, TP26 (GND)", "TP19 (LEDOUT), TP20 (LED)", "TP21 (BUTTON)", "TP27..TP34 (D0..D7)", "TP35..TP44 (E0..E9)"],
    description: "Внешний интерфейс подключения материнской платы к домофонной магистрали, координатно-матричным шлейфам 8×10, кнопке сервисного режима MODE и адресному RGB-светодиоду состояния.",
    notes: "Перемычка R1 (0 Ом) объединяет линию OpenD с выходом SW-OUT2 коммутатора E10 для аппаратного открытия двери."
  },
  {
    id: 2,
    code: "Узел 02",
    title: "Интерфейс модуля речевого оповещения Левитан",
    schematicLabel: "LeviAMP PCB (XS2 / XS3)",
    category: "Интерфейсы и связь",
    sheetPages: [2],
    sheetName: "Switcher.SchDoc (Лист 2)",
    components: ["XS2 (LeviAMP, 16-pin)", "XS3 (AMP, 8-pin)", "RN1 (0 Ом×4, -Amp via E10)"],
    signalsIn: ["+Vamp", "SOUND1, SOUND2", "ENAMP1..ENAMP3", "ENREL1, ENREL2", "LEDAMP"],
    signalsOut: ["AMP1..AMP8", "ALARM-IN"],
    testPoints: ["TP10..TP16 (SOUND/ENAMP/ENREL)", "TP18 (GND)", "TP22 (LEDAMP)", "TP23 (ALARM-IN)", "TP24 (+VAMP)"],
    description: "Разъемы сопряжения с дочерней платой усилителя оповещения Левитан. Передает аналоговый звук (DAC1/DAC2), питание +Vamp, сигналы включения каналов усилителя и коммутируемые релейные выходы AMP1..AMP8 на линии десятков."
  },
  {
    id: 3,
    code: "Узел 03",
    title: "Дифференциальный трансивер шины RS-485",
    schematicLabel: "RS-485",
    category: "Интерфейсы и связь",
    sheetPages: [3],
    sheetName: "Com-485-LU-1W.SchDoc (Лист 3)",
    components: ["DD2 (MAX13487EESA+)", "FR1, FR2 (100мА/30В/15Ом)", "FS1 (TVS 5В/16А)", "R4, R6 (1.5 кОм)", "R5 (120 Ом)", "C1, C2 (18 пФ)", "RN5 (10 кОм×4)"],
    signalsIn: ["A, B (линия)", "DI, RE, nSHDN", "+5VL, +3.3V"],
    signalsOut: ["RS485_A, RS485_B", "RO"],
    testPoints: ["TP48 (RS-485-A)", "TP49 (RS-485-B)", "TP52 (DI)", "TP53 (RO)", "TP54 (RE)", "TP55 (nSHDN)"],
    description: "Обеспечивает помехоустойчивую полудуплексную связь с домофоном по RS-485 с автоматическим управлением направлением (AutoDirection), согласующим терминатором R5 (120 Ом), самовосстанавливающимися предохранителями FR1/FR2 и TVS-защитой FS1."
  },
  {
    id: 4,
    code: "Узел 04",
    title: "Узел детекции статуса трубок LU1 / LU2",
    schematicLabel: "LU",
    category: "Интерфейсы и связь",
    sheetPages: [3],
    sheetName: "Com-485-LU-1W.SchDoc (Лист 3)",
    components: ["DD1 (74LVC2G17GW,125)", "R2, R3 (510 кОм)", "R7, R8 (1 МОм)", "VD1, VD2 (300мА/40В)", "C3, C4 (18 пФ)", "C6 (100 нФ)", "RN4 (1 кОм×2)"],
    signalsIn: ["LU1, LU2 (вход линии)", "+3.3V"],
    signalsOut: ["DO1, DO2 (в МК)"],
    testPoints: ["TP45 (LU1)", "TP46 (LU2)", "TP50 (LU-DO1)", "TP51 (LU-DO2)"],
    description: "Входной формирователь дискретных сигналов снятия/положения абонентской трубки. Высокоомные делители с диодным ограничением на шину +3.3В защищают входы двойного триггера Шмитта 74LVC2G17 от перенапряжений и подавляют дребезг."
  },
  {
    id: 5,
    code: "Узел 05",
    title: "Каскад 1-Wire и управления открытием двери",
    schematicLabel: "1W",
    category: "Интерфейсы и связь",
    sheetPages: [3],
    sheetName: "Com-485-LU-1W.SchDoc (Лист 3)",
    components: ["VT1 (N-MOSFET 1А/60В)", "VT2, VT3 (BC807K-40R)", "VD3 (300мА/40В)", "RN2 (1 кОм×2)", "RN3 (22 Ом×4)", "RN6 (10 кОм×4)"],
    signalsIn: ["1WTx", "+5VL"],
    signalsOut: ["OpenD (Open/1W)", "1WRx"],
    testPoints: ["TP47 (1W-G)", "TP56 (1W выход)", "TP57 (1WRx)"],
    description: "Двунаправленный силовой буфер с открытым стоком на VT1 и биполярной схемой считывания уровня на VT2/VT3. Служит для обмена по шине 1-Wire и подачи импульса открытия замка на линию OpenD."
  },
  {
    id: 6,
    code: "Узел 06",
    title: "Центральный микроконтроллер ESP32 и RF-тракт",
    schematicLabel: "MCU / RF",
    category: "Ядро МК",
    sheetPages: [4],
    sheetName: "MCU-ESP.SchDoc (Лист 4)",
    components: ["DD4 (ESP32-PICO-V3)", "ANT1 (антенна 2.4 ГГц)", "L1 (2.2 нГн)", "L2 (1.8 нГн)", "C16 (1.5 пФ)", "C19 (1.2 пФ)", "C11..C15, C17..C21", "NT2..NT4", "RN15..RN19"],
    signalsIn: ["+3.3V", "U0RX, U1RxD", "DO1, DO2", "ADC0-6, ADC1-6", "INT0..INT2"],
    signalsOut: ["DAC1, DAC2", "SPI (MOSI, MISO, CLK, CS0..2, EN0/2)", "I2C (SDA, SCL)", "LED"],
    testPoints: ["TP84 (EN)", "TP85..TP102 (GPIO/ADC/DAC/I2C)", "TP111..TP121 (SPI/UART/GPIO21)"],
    description: "Вычислительное и беспроводное ядро платы (Wi-Fi 802.11 b/g/n + BLE) с раздельными цепями фильтрации питания VDDA3P3, VDDCPU, VDDRTC и П-образной согласующей цепью L2/C16/C19 для встроенной антенны ANT1."
  },
  {
    id: 7,
    code: "Узел 07",
    title: "I2C-расширитель портов и матрица перемычек UART/1W",
    schematicLabel: "DIGITAL-EXPANDER / RS-485 SW / 1W-SW / I2C-L",
    category: "Ядро МК",
    sheetPages: [4],
    sheetName: "MCU-ESP.SchDoc (Лист 4)",
    components: ["DD3 (TCA9535PWR)", "R12..R15, R20, R21 (1 кОм, адреса I2C)", "RN7, RN8, RN10..RN13 (1 кОм×2)", "R11 (1 кОм)", "C8 (100 нФ)", "VD4, R9, R10, C7 (BUTTON)"],
    signalsIn: ["SDA, SCL", "+3.3V", "SW1 (BUTTON)"],
    signalsOut: ["POWER (PG, V1EN, V2EN, VAMPON, eLOADEN)", "D8 (A0..A2, A5..A6, EN)", "ENA1..ENA5, LEDAMP, ALARMIN", "INT0"],
    testPoints: ["TP58, TP70 (BUTTON)", "TP59..TP69 (POWER/D8)", "TP71..TP73 (EXPA0..2)", "TP74..TP80 (AMP)"],
    description: "16-канальный I2C-расширитель GPIO управляет линиями включения стабилизаторов, адресацией коммутатора D8 и каналами платы Левитан. Сборки RN7–RN13 задают аппаратную маршрутизацию линий UART0/UART1/GPIO между RS-485, 1-Wire и I2C."
  },
  {
    id: 8,
    code: "Узел 08",
    title: "Датчики VAMPOK, VersionPCB и интерфейсы отладки",
    schematicLabel: "VAMPOK / VersionPCB / AMPCONN / JTAG / MCU-program",
    category: "Ядро МК",
    sheetPages: [4],
    sheetName: "MCU-ESP.SchDoc (Лист 4)",
    components: ["R16 (510 кОм), R17 (75 кОм), VD5, C9 (VAMPOK)", "R18 (75 кОм), R19 (1 кОм), R22 (4.7 кОм), C10 (VersionPCB)", "FS2..FS9 (TVS)", "RN9, RN14 (22 Ом×4)", "XP4 (JTAG 10-pin)", "XP5 (Prog 7-pin)"],
    signalsIn: ["+Vamp", "+3.3V", "JTMS/JTCK/JTDO/JTDI", "Boot/EN/Tx/Rx"],
    signalsOut: ["ADC0-6 (VAmpM)", "ADC1-6 (Version)", "DAC1/2, ENA1..3, LEDAMP, ALARMIN"],
    testPoints: ["TP81 (VAmpM), TP83 (VAMP)", "TP82 (Version)", "TP103..TP106 (Prog)", "TP107..TP110 (JTAG)", "TP122 (GND)"],
    description: "Аналоговые делители контролируют напряжение питания усилителя +Vamp и аппаратную ревизию печатной платы через каналы АЦП ESP32. Разъемы XP4 и XP5 с TVS-сборками FS6–FS9 предназначены для прошивки и внутрисхемной отладки JTAG."
  },
  {
    id: 9,
    code: "Узел 09",
    title: "Коммутатор десятков и диодная защита линий D0–D7",
    schematicLabel: "COMM-D",
    category: "Матричная коммутация",
    sheetPages: [5],
    sheetName: "D8.SchDoc (Лист 5)",
    components: ["DA1 (ADG1608BR)", "VD6..VD14 (диодные сборки 300мА/40В)", "C24 (100 нФ)"],
    signalsIn: ["SW-A0, SW-A1, SW-A2, SW-EN", "+LINE", "+Vp"],
    signalsOut: ["D0..D7"],
    testPoints: ["TP123..TP126 (SWD8-A0..EN)", "TP127 (SWD8-Line)", "TP128 (SWD8-Vp)"],
    description: "Низкоомный 8-канальный аналоговый мультиплексор ADG1608BR (R_ON ≈ 1.5 Ом) подключает общую линию +LINE к одной из 8 шин десятков D0–D7. Защитные диоды VD6–VD14 ограничивают выбросы напряжения в пределах от GND до +Vp."
  },
  {
    id: 10,
    code: "Узел 10",
    title: "Узел защиты от «перевертышей» и смещения VOffset",
    schematicLabel: "VOffset",
    category: "Матричная коммутация",
    sheetPages: [5],
    sheetName: "D8.SchDoc (Лист 5)",
    components: ["D1 (MC33879APEKR2)", "VD15..VD22 (20В/415мВ@1А)", "VD23 (25В/450мВ@1А)", "RN20, RN21 (1 кОм×2)", "RN24, RN25 (0 Ом×4, уст.)", "RN22, RN23, RN26 (DNP — не уст.)", "C22, C23, C25..C27"],
    signalsIn: ["SPI (MOSI, MISO, CLK, CS-D, EN-DE)", "D8 (A5, A6)", "+VD, +Vp, +V2", "AMP1..AMP8"],
    signalsOut: ["D0..D7 (смещение/оповещение)", "VOHUP, VOHDN"],
    testPoints: ["TP129..TP136 (SPI / IN5..6 / +VD)", "TP137..TP144 (VOD1..VOD8)", "TP145 (VOH-UP)", "TP146 (VOH-DN)"],
    description: "8-канальный SPI-драйвер MC33879 управляет подачей напряжения смещения и маршрутизацией сигналов голосового оповещения AMP1..AMP8 через развязывающие диоды Шоттки VD15–VD22, блокируя паразитные токи при неверной полярности абонентской трубки."
  },
  {
    id: 11,
    code: "Узел 11",
    title: "Коммутатор единиц E0–E9 и фильтр возврата -LINE",
    schematicLabel: "E10 / POWER",
    category: "Матричная коммутация",
    sheetPages: [6],
    sheetName: "E10.SchDoc (Лист 6)",
    components: ["D2 (TLE94112EL)", "VD24 (диодный мост 1кВ/2А)", "L3 (1 мГн/150 мА)", "R23 (1 Ом)", "C28 (10 мкФ/50В)", "C29, C30"],
    signalsIn: ["SPI (MOSI, MISO, CLK, CS-E, EN-DE)", "VS1, VS2, +V1, +3.3V", "-LINE"],
    signalsOut: ["E0..E9", "SW-OUT2 (OpenD)", "SW-OUT12"],
    testPoints: ["TP147 (GNDE)", "TP148 (+V1 / C28)", "TP149 (VS1)", "TP150 (VS2)", "TP151 (SWE-O11)", "TP152 (SWE-O12)"],
    description: "12-канальный полумостовой драйвер TLE94112EL формирует низковольтную координату единиц E0..E9 для 80-абонентской матрицы, а также управляет ключами SW-OUT2/SW-OUT12. Диодный мост VD24 и дроссель L3 с резистором R23 фильтруют возвратную линию -LINE."
  },
  {
    id: 12,
    code: "Узел 12",
    title: "Входная защита питания и ключ усилителя +Vamp",
    schematicLabel: "POWERIN",
    category: "Питание и телеметрия",
    sheetPages: [7],
    sheetName: "Power.SchDoc (Лист 7)",
    components: ["FS10 (TVS 12В/600Вт)", "VT5 (P-MOSFET AO3407)", "VT4 (N-MOSFET 2N7002K)", "VD25 (60В/2А)", "VD26 (стабилитрон 16В/1Вт)", "R24 (10 кОм)", "R25 (0 Ом)", "C33", "NT5..NT7"],
    signalsIn: ["+VIN, -VPIN (12–24В)", "VAMPON (от DD3)"],
    signalsOut: ["+Vdc", "+Vp", "+Vamp"],
    testPoints: ["TP153 (V-AMO-nON)", "TP154 (G-VT5)", "TP155 (+VAMP)", "TP157 (+Vp)", "TP158 (-VPIN)", "TP161 (PGND)"],
    description: "Узел входного питания с мощным TVS-супрессором FS10, развязкой силовой и сигнальной земель (NT5–NT7) и транзисторным ключом VT4/VT5 для программного включения питания платы усилителя +Vamp."
  },
  {
    id: 13,
    code: "Узел 13",
    title: "Основной 3-канальный DC-DC преобразователь",
    schematicLabel: "DC-DC / DC2 / DC3V",
    category: "Питание и телеметрия",
    sheetPages: [7],
    sheetName: "Power.SchDoc (Лист 7)",
    components: ["D3 (TPS65261RHBR)", "FR3..FR5 (900мА/24В)", "L4 (4.7 мкГн/2А)", "L5, L6 (15 мкГн/1А)", "VD27 (3.3В/500мВт)", "R26..R34", "C31..C50, C53", "NT8"],
    signalsIn: ["+Vdc", "V1EN, V2EN", "FB1, FB2"],
    signalsOut: ["+3.3V / +3V3M", "+VDC_MV1", "+VDC_MV2", "PG (Power Good)"],
    testPoints: ["TP156 (VDIV)", "TP159..TP160, TP164 (SS1..3)", "TP162..TP163, TP166 (PVIN)", "TP165 (ROSC)", "TP167 (PG)", "TP168 (V7V)", "TP169..TP177 (COMP/LX/FB)", "TP178 (+3V3M)", "TP182 (+3V3)", "TP183 (GND)"],
    description: "Тройной синхронный понижающий регулятор TPS65261RHBR формирует три независимых напряжения: постоянную логическую шину +3.3В (канал 3) и две динамически регулируемые шины +VDC_MV1 / +VDC_MV2 (каналы 1 и 2)."
  },
  {
    id: 14,
    code: "Узел 14",
    title: "Вспомогательные DC-DC L1/V5 и коррекция порога L1ADJ",
    schematicLabel: "DC1 (L1) / DC4 (V5) / L1ADJ",
    category: "Питание и телеметрия",
    sheetPages: [7, 10, 11],
    sheetName: "Power.SchDoc (Лист 7) + DC-DC.SchDoc (Листы 9.1, 9.2)",
    components: ["DP1, DP2 (LM22675MRX-5.0/NOPB)", "DV1 (TL431ACDCKR)", "L7, L8 (22 мкГн/1.93А)", "VD28, VD29", "R35..R38", "R39, R40 (шунты 100 мОм)", "C51, C52, C72..C81", "R74, R75"],
    signalsIn: ["+Vdc", "Uload", "CEN_L1, CEN_V5"],
    signalsOut: ["+5VL (TP179)", "+VCO_L1 (Ufb=1.285В)"],
    testPoints: ["TP179 (+5V)"],
    description: "Два импульсных регулятора LM22675 формируют шину +5VL (узел V5, лист 11) и вспомогательную шину L1 (лист 10). Схема L1ADJ на прецизионном стабилитроне TL431 и шунтах R39/R40 (100 мОм) стабилизирует выходной ток при падении 0.1–0.2 В."
  },
  {
    id: 15,
    code: "Узел 15",
    title: "Каналы цифрового управления и телеметрии MV1, MV1e, MV2",
    schematicLabel: "ManageChannel (MV1 / MV1e / MV2)",
    category: "Питание и телеметрия",
    sheetPages: [8, 9, 12],
    sheetName: "ManageChannel.SchDoc (Листы 8, 9, 10)",
    components: ["DA2, DA3, DA4 (MCP4532T-104E/MS)", "DD5, DD6 (INA226AIDGSR)", "DD7 (DNP — не уст.)", "R51, R73, R62 (шунты 20 мОм±1%)", "C54..C71"],
    signalsIn: ["+VDC_MV1, +V1, +VDC_MV2", "SDA, SCL, +3V3M"],
    signalsOut: ["+V1 (TP180), Uload, +V2 (TP181)", "FB1, Uref, FB2", "INT1, INT2"],
    testPoints: ["TP184..TP189 (MV1)", "TP196..TP201 (MV1e)", "TP190..TP195 (MV2)"],
    description: "Три идентичных модуля подстройки и контроля питания. Цифровые потенциометры MCP4532 меняют делитель обратной связи DC-DC для плавной регулировки +V1/+V2/Uload по I2C, а 16-битные мониторы INA226 с шунтами 20 мОм измеряют ток, напряжение и мощность с формированием прерываний INT1/INT2."
  },
  {
    id: 16,
    code: "Узел 16",
    title: "Конструкция печатной платы, стек слоев и топология",
    schematicLabel: "Mechanical / PCBStack / Gerber",
    category: "Конструкция ПП",
    sheetPages: [13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23],
    sheetName: "Mechanical / LeviCom-MB / PCBStack (Листы 13–25)",
    components: ["4-слойный FR4 (1.587 мм)", "Топология GTL / G1 (GND) / G2 (Power) / GBL", "Слои шелкографии GTO / GBO", "Слои AUX (GM9..GM22)"],
    signalsIn: ["Реперные знаки", "Крепежные отверстия"],
    signalsOut: ["Сборочный чертеж 1:1", "3D-модель сборки"],
    testPoints: ["Все TP1..TP201 сгруппированы на Top Overlay (лист 21)"],
    description: "Конструкторская документация на 4-слойную печатную плату толщиной 1.587 мм (диэлектрик FR4, медь 35 мкм / 1 oz). Содержит порядок пайки (сначала SMT, затем TH от центра к краям), 3D-виды и полную карту контрольных точек."
  }
];

export const DATASHEETS: DatasheetRecord[] = [
  {
    ref: "DD4",
    name: "ESP32-PICO-V3",
    manufacturer: "Espressif Systems",
    package: "LGA-48 (7×7 мм)",
    voltage: "3.0В – 3.6В",
    interface: "SPI, I2C, UART, Wi-Fi, BLE",
    sheetPages: [4],
    description: "Модуль SiP на базе двухъядерного процессора ESP32 со встроенной флеш-памятью, кварцевым генератором и полным набором периферии.",
    keyFeatures: ["Двухъядерный процессор Tensilica LX6 до 240 МГц", "Wi-Fi 802.11 b/g/n и Bluetooth 4.2 BR/EDR & BLE", "Встроенные 12-битные АЦП (ADC0-6, ADC1-6) и ЦАП (DAC1, DAC2)"]
  },
  {
    ref: "DD3",
    name: "TCA9535PWR",
    manufacturer: "Texas Instruments",
    package: "24-pin TSSOP",
    voltage: "1.65В – 5.5В",
    interface: "I2C / SMBus + прерывание INT",
    sheetPages: [4],
    description: "16-канальный I2C-расширитель портов ввода/вывода общего назначения с выходом прерывания с открытым стоком.",
    keyFeatures: ["16 конфигурируемых линий P00–P07 и P10–P17", "Выбор адреса перемычками A0, A1, A2 (R12–R15, R20–R21)", "Управление питанием (V1EN, V2EN, VAMPON), D8 и усилителем"]
  },
  {
    ref: "DD2",
    name: "MAX13487EESA+",
    manufacturer: "Analog Devices / Maxim",
    package: "8-pin SOIC",
    voltage: "4.75В – 5.25В",
    interface: "RS-485 / RS-422",
    sheetPages: [3],
    description: "Полудуплексный дифференциальный трансивер RS-485 со схемой AutoDirection для автоматического переключения прием/передача.",
    keyFeatures: ["Аппаратное автоуправление направлением (AutoDirection)", "Защита от электростатического разряда ESD ±15 кВ", "Поддержка горячего подключения (Hot-Swap) и режим сна nSHDN"]
  },
  {
    ref: "DD1",
    name: "74LVC2G17GW,125",
    manufacturer: "Nexperia",
    package: "6-pin TSSOP6 / SOT363",
    voltage: "1.65В – 5.5В",
    interface: "Двойной буфер (триггер Шмитта)",
    sheetPages: [3],
    description: "Двухканальный неинвертирующий буфер с триггером Шмитта на входах для надежной цифровой фильтрации сигналов линии трубок LU1 и LU2.",
    keyFeatures: ["Гистерезис на входах для защиты от помех и дребезга", "Толерантные к 5В входы при питании от 3.3В", "Выходной ток до ±24 мА при 3.3В"]
  },
  {
    ref: "DA1",
    name: "ADG1608BR",
    manufacturer: "Analog Devices",
    package: "16-pin TSSOP",
    voltage: "+3.3В..+16В / ±5В",
    interface: "3-битный адрес (A0–A2) + EN",
    sheetPages: [5],
    description: "Монолитный 8-канальный аналоговый мультиплексор с ультранизким сопротивлением открытого канала для коммутации десятков D0–D7.",
    keyFeatures: ["Сопротивление открытого канала R_ON ≈ 1.5 Ом", "Непрерывный ток через замкнутый ключ до 300 мА", "Малая неравномерность сопротивления по всему диапазону"]
  },
  {
    ref: "D1",
    name: "MC33879APEKR2",
    manufacturer: "NXP Semiconductors",
    package: "32-pin SOICW-EP",
    voltage: "5.0В (логика), до 28В (нагрузка)",
    interface: "SPI + входы прямого ШИМ IN5/IN6",
    sheetPages: [5],
    description: "8-канальный конфигурируемый драйвер верхнего/нижнего плеча с защитой от КЗ и диагностикой по SPI для узла смещения VOffset.",
    keyFeatures: ["8 независимых силовых выходов с ограничением тока", "Диагностика обрыва и короткого замыкания по SPI", "Защита от перегрева и обратного выброса напряжения"]
  },
  {
    ref: "D2",
    name: "TLE94112EL",
    manufacturer: "Infineon Technologies",
    package: "24-pin SSOP-EP",
    voltage: "3.3В/5В (логика), 5.5В–18В (VS1/VS2)",
    interface: "SPI (до 5 МГц)",
    sheetPages: [6],
    description: "12-канальный полумостовой драйвер, формирующий координату единиц E0–E9 и силовые выходы SW-OUT2 / SW-OUT12.",
    keyFeatures: ["12 независимых полумостовых выходов (0.9А каждый)", "Полная поканальная диагностика по SPI", "Независимое питание силовых банков VS1 и VS2"]
  },
  {
    ref: "D3",
    name: "TPS65261RHBR",
    manufacturer: "Texas Instruments",
    package: "32-pin VQFN (5×5 мм)",
    voltage: "4.5В – 18В",
    interface: "EN1–EN3, FB1–FB3, PGOOD",
    sheetPages: [7],
    description: "Синхронный понижающий трехканальный DC-DC преобразователь, формирующий +3.3В и две управляемые шины +VDC_MV1 / +VDC_MV2.",
    keyFeatures: ["Три синхронных канала (3А / 2А / 2А)", "Программируемый плавный старт (SS1–SS3) и частота (ROSC)", "Сигнал готовности питания Power Good (PG)"]
  },
  {
    ref: "DD5, DD6, DD7*",
    name: "INA226AIDGSR",
    manufacturer: "Texas Instruments",
    package: "10-pin VSSOP",
    voltage: "2.7В – 5.5В (до 36В на шине)",
    interface: "I2C / SMBus + выход Alert",
    sheetPages: [8, 9, 12],
    description: "Прецизионный 16-битный двунаправленный монитор тока, напряжения и мощности. (*DD7 в канале MV1e на плате не устанавливается).",
    keyFeatures: ["Измерение падения на шунте 20 мОм ±1% с разрешением 2.5 мкВ", "Аппаратный расчет тока и мощности в мВт", "Программируемый компаратор тревоги Alert (INT1/INT2)"]
  },
  {
    ref: "DA2, DA3, DA4",
    name: "MCP4532T-104E/MS",
    manufacturer: "Microchip Technology",
    package: "8-pin MSOP",
    voltage: "2.7В – 5.5В",
    interface: "I2C (до 3.4 МГц)",
    sheetPages: [8, 9, 12],
    description: "Цифровой реостат 100 кОм (129 позиций, 7 бит) в цепи обратной связи DC-DC для программной регулировки напряжений +V1, +V2 и Uload.",
    keyFeatures: ["Номинальное сопротивление 100 кОм, 128 шагов", "Выбор I2C-адреса резисторами A0/A1", "Плавная коррекция выходного напряжения без переключения реле"]
  },
  {
    ref: "DP1, DP2",
    name: "LM22675MRX-5.0/NOPB",
    manufacturer: "Texas Instruments",
    package: "8-pin SOIC-EP (PowerPAD)",
    voltage: "4.5В – 42В",
    interface: "Enable + обратная связь FB",
    sheetPages: [10, 11],
    description: "Импульсный понижающий стабилизатор серии SIMPLE SWITCHER (до 1А), формирующий шины +5VL (DP2) и +VCO_L1 (DP1).",
    keyFeatures: ["Широкий диапазон входных напряжений до 42В", "Частота преобразования 500 кГц", "Интегрированный силовой ключ и плавный старт"]
  },
  {
    ref: "DV1",
    name: "TL431ACDCKR",
    manufacturer: "Texas Instruments",
    package: "6-pin SC70 / SOT-363",
    voltage: "Uref = 2.495В..36В",
    interface: "Аналоговая ОС (узел L1ADJ)",
    sheetPages: [7],
    description: "Программируемый прецизионный параллельный стабилизатор в узле коррекции обратной связи L1ADJ на шунтах R39/R40 (100 мОм).",
    keyFeatures: ["Точность опорного напряжения ±1%", "Работает совместно с преобразователем L1 (DP1)", "Ограничение и стабилизация тока нагрузки по падению напряжения"]
  }
];

export const NETS_LIST: NetRecord[] = [
  { net: "+VIN / -VPIN", category: "Питание", sheets: "Листы 2, 7", description: "Входное питание платы 12–24В через клеммник XP1 (защита TVS FS10)" },
  { net: "+5VL", category: "Питание", sheets: "Листы 2, 3, 7, 11", description: "Шина питания +5В для трансивера RS-485 и каскада 1-Wire (от DP2)" },
  { net: "+3.3V / +3V3M", category: "Питание", sheets: "Листы 2–9, 12", description: "Основное питание логики, ESP32-PICO-V3, расширителя TCA9535 и измерителей INA226" },
  { net: "+Vp", category: "Питание", sheets: "Листы 2, 5, 7", description: "Питание мультиплексора D8 (DA1), защитных диодов VD6–VD14 и драйвера D1" },
  { net: "+Vamp", category: "Питание", sheets: "Листы 2, 4, 7", description: "Коммутируемое ключом VT4/VT5 питание платы речевого оповещения Левитан" },
  { net: "+V1, +V2, Uload", category: "Питание", sheets: "Листы 2, 5–10, 12", description: "Программируемые силовые шины матрицы и нагрузки с телеметрией по I2C" },
  { net: "RS485_A / RS485_B", category: "Связь", sheets: "Листы 2, 3", description: "Дифференциальная линия интерфейса RS-485 с терминатором 120 Ом (R5)" },
  { net: "LU1, LU2 / DO1, DO2", category: "Связь", sheets: "Листы 2, 3, 4", description: "Аналоговые входы детекции состояния трубок и их цифровые выходы после DD1" },
  { net: "OpenD (1WRx / 1WTx)", category: "Связь", sheets: "Листы 2, 3, 4", description: "Сервисная шина 1-Wire и выход управления открытием двери" },
  { net: "D0..D7", category: "Матрица", sheets: "Листы 2, 5", description: "8 координатных линий выбора десятков абонентов матрицы 8×10" },
  { net: "E0..E9", category: "Матрица", sheets: "Листы 2, 6", description: "10 координатных линий выбора единиц абонентов матрицы 8×10" },
  { net: "+LINE / -LINE", category: "Матрица", sheets: "Листы 2, 5, 6", description: "Абонентская разговорная магистраль с мостом VD24 и фильтром L3/R23" },
  { net: "SDA / SCL (INT0..2)", category: "Управление", sheets: "Листы 2, 4, 7–9, 12", description: "Шина I2C управления расширителем DD3, потенциометрами DA2–DA4 и АЦП DD5–DD6" },
  { net: "SPI (MOSI, MISO, CLK, CS)", category: "Управление", sheets: "Листы 2, 4, 5, 6", description: "Шина SPI управления драйверами матрицы D1 (MC33879) и D2 (TLE94112)" }
];
