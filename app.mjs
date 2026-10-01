import { MAX_INPUT_BYTES, convertSave } from './converter.mjs';

const copy = {
  en: {
    pageTitle: 'RGH Save Converter · Lost Odyssey Recomp',
    brandLabel: 'Lost Odyssey Recomp on GitHub',
    navLabel: 'Page links',
    languageLabel: 'Language',
    issueLink: 'Issue #88',
    eyebrow: 'SAVE TRANSFER / RGH → PC',
    title: 'RGH save converter',
    intro: 'Convert a Lost Odyssey RGH save to a Recomp ZIP.',
    privacy: 'Converted locally. No uploads.',
    inputKicker: 'SOURCE & DESTINATION',
    inputTitle: 'Prepare the save',
    fileLabel: 'RGH save file',
    dropTitle: 'Drop a file here',
    dropSubtitle: 'or choose a file',
    fileHint: 'CON save (e.g. user00) or ZIP · up to 16 MiB',
    slotLabel: 'Destination save slot',
    slotHint: 'Choose an empty slot to avoid overwriting a save.',
    convertButton: 'Convert save',
    convertingButton: 'Converting…',
    errorTitle: 'Could not convert this save',
    resultKicker: 'OUTPUT',
    resultTitle: 'Download & install',
    emptyResult: 'No file converted yet.',
    readyLabel: 'READY TO DOWNLOAD',
    sourceFact: 'Source',
    slotFact: 'Destination',
    sizeFact: 'Save data',
    downloadButton: 'Download ZIP',
    detailsSummary: 'File details',
    hashLabel: 'Save data SHA-256',
    installTitle: 'Import the save',
    installBackup: 'Close the game and back up your save folder.',
    installWindows: 'Windows portable: extract beside the game executable.',
    installLinux: 'Linux: copy the ZIP’s userNN folder into your save folder.',
    installGuide: 'Find your save folder.',
    guideHref: 'https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/INSTALLING.md#file-locations',
    supportNote: 'Supports the verified Lost Odyssey CON format. RGH → PC only.',
    footerIssue: 'Conversion notes · Issue #88',
    selected: 'File selected.',
    converting: 'Converting…',
    converted: 'Conversion complete.',
    downloaded: 'ZIP download started.',
    tooLarge: 'This file is larger than the 16 MiB limit.',
    emptyFile: 'This file is empty.',
    multipleFiles: 'Choose one file at a time.',
    unknownError: 'An unexpected error occurred.',
    untitledSave: 'Lost Odyssey save',
    slotOption: (number, slot) => `Slot ${number} · ${slot}`,
    slotResult: (number, slot) => `Slot ${number} · save/${slot}`,
    fileChosen: (name, size) => `${name} · ${size}`,
    bytes: (size) => `${new Intl.NumberFormat('en').format(size)} bytes`,
  },
  zh: {
    pageTitle: 'RGH 存档转换 · Lost Odyssey Recomp',
    brandLabel: '在 GitHub 查看 Lost Odyssey Recomp',
    navLabel: '页面链接',
    languageLabel: '语言',
    issueLink: '议题 #88',
    eyebrow: '存档转移 / RGH → PC',
    title: 'RGH 存档转换',
    intro: '转换《失落的奥德赛》RGH 存档，下载 Recomp 存档 ZIP。',
    privacy: '本地转换，不上传存档。',
    inputKicker: '来源与目标',
    inputTitle: '准备存档',
    fileLabel: 'RGH 存档文件',
    dropTitle: '将文件拖到这里',
    dropSubtitle: '或选择文件',
    fileHint: 'CON 存档（如 user00）或 ZIP · 最大 16 MiB',
    slotLabel: '目标存档槽位',
    slotHint: '选择空槽位，避免覆盖已有存档。',
    convertButton: '转换存档',
    convertingButton: '转换中…',
    errorTitle: '无法转换这个存档',
    resultKicker: '输出',
    resultTitle: '下载并安装',
    emptyResult: '等待转换',
    readyLabel: '可以下载',
    sourceFact: '来源文件',
    slotFact: '目标位置',
    sizeFact: '存档数据',
    downloadButton: '下载 ZIP',
    detailsSummary: '文件详情',
    hashLabel: '存档数据 SHA-256',
    installTitle: '导入存档',
    installBackup: '关闭游戏，备份 save 文件夹。',
    installWindows: 'Windows 便携版：解压到游戏程序旁。',
    installLinux: 'Linux：将 ZIP 内的 userNN 文件夹放入存档目录。',
    installGuide: '查找存档目录。',
    guideHref: 'https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/INSTALLING.zh-CN.md#file-locations',
    supportNote: '支持已验证的 CON 格式，仅限 RGH → PC。',
    footerIssue: '转换说明 · 议题 #88',
    selected: '已选择文件。',
    converting: '转换中…',
    converted: '转换完成。',
    downloaded: '已开始下载 ZIP。',
    tooLarge: '文件超过 16 MiB 限制。',
    emptyFile: '文件为空。',
    multipleFiles: '每次只能选择一个文件。',
    unknownError: '发生了意外错误。',
    untitledSave: '失落的奥德赛存档',
    slotOption: (number, slot) => `槽位 ${number} · ${slot}`,
    slotResult: (number, slot) => `槽位 ${number} · save/${slot}`,
    fileChosen: (name, size) => `${name} · ${size}`,
    bytes: (size) => `${new Intl.NumberFormat('zh-CN').format(size)} 字节`,
  },
};

const form = document.querySelector('#converter-form');
const fileInput = document.querySelector('#source-file');
const dropzone = document.querySelector('#dropzone');
const slotSelect = document.querySelector('#target-slot');
const chosenFile = document.querySelector('#chosen-file');
const convertButton = document.querySelector('#convert-button');
const convertLabel = document.querySelector('#convert-label');
const statusMessage = document.querySelector('#status-message');
const errorBox = document.querySelector('#error-box');
const errorMessage = document.querySelector('#error-message');
const resultEmpty = document.querySelector('#result-empty');
const resultReady = document.querySelector('#result-ready');
const downloadButton = document.querySelector('#download-button');
const saveDisplayName = document.querySelector('#save-display-name');
const sourceName = document.querySelector('#source-name');
const outputSlot = document.querySelector('#output-slot');
const payloadSize = document.querySelector('#payload-size');
const payloadHash = document.querySelector('#payload-hash');
const languageButtons = {
  en: document.querySelector('#lang-en'),
  zh: document.querySelector('#lang-zh'),
};

let language = navigator.language?.toLowerCase().startsWith('zh') ? 'zh' : 'en';
let sourceFile = null;
let output = null;
let phase = 'empty';
let currentError = '';
let generation = 0;

for (let index = 0; index < 30; index += 1) {
  const slot = `user${String(index).padStart(2, '0')}`;
  const option = document.createElement('option');
  option.value = slot;
  slotSelect.append(option);
}

function displaySlot(slot) {
  const number = String(Number(slot.slice(4)) + 1).padStart(2, '0');
  return copy[language].slotResult(number, slot);
}

function render() {
  const strings = copy[language];
  document.documentElement.lang = language === 'zh' ? 'zh-CN' : 'en';
  document.title = strings.pageTitle;
  document.querySelector('.brand').setAttribute('aria-label', strings.brandLabel);
  document.querySelector('.header-links').setAttribute('aria-label', strings.navLabel);
  document.querySelector('.language-switch').setAttribute('aria-label', strings.languageLabel);
  document.querySelector('#install-guide').href = strings.guideHref;

  for (const element of document.querySelectorAll('[data-i18n]')) {
    element.textContent = strings[element.dataset.i18n];
  }
  for (const [code, button] of Object.entries(languageButtons)) {
    button.setAttribute('aria-pressed', String(code === language));
  }
  for (const [index, option] of [...slotSelect.options].entries()) {
    option.textContent = strings.slotOption(String(index + 1).padStart(2, '0'), option.value);
  }

  chosenFile.hidden = !sourceFile;
  chosenFile.textContent = sourceFile ? strings.fileChosen(sourceFile.name, strings.bytes(sourceFile.size)) : '';
  const busy = phase === 'busy';
  fileInput.disabled = busy;
  slotSelect.disabled = busy;
  convertButton.disabled = busy || !sourceFile;
  convertButton.classList.toggle('is-busy', busy);
  dropzone.classList.toggle('is-disabled', busy);
  form.setAttribute('aria-busy', String(busy));
  convertLabel.textContent = busy ? strings.convertingButton : strings.convertButton;

  statusMessage.textContent = {
    selected: strings.selected,
    busy: strings.converting,
    success: strings.converted,
    downloaded: strings.downloaded,
  }[phase] ?? '';
  errorBox.hidden = phase !== 'error';
  errorMessage.textContent = currentError;

  const ready = Boolean(output);
  resultEmpty.hidden = ready;
  resultReady.hidden = !ready;
  downloadButton.hidden = !ready;
  if (ready) {
    const summary = output.summary;
    saveDisplayName.textContent = summary.displayName || strings.untitledSave;
    sourceName.textContent = summary.sourceName || sourceFile?.name || '';
    outputSlot.textContent = displaySlot(summary.slot);
    payloadSize.textContent = strings.bytes(summary.payloadBytes);
    payloadHash.textContent = summary.payloadSha256;
  }
}

function invalidateResult() {
  generation += 1;
  output = null;
  currentError = '';
  phase = sourceFile ? 'selected' : 'empty';
  saveDisplayName.textContent = '';
  sourceName.textContent = '';
  outputSlot.textContent = '';
  payloadSize.textContent = '';
  payloadHash.textContent = '';
}

function showError(message) {
  output = null;
  phase = 'error';
  currentError = message;
  render();
}

function chooseFile(file) {
  if (phase === 'busy') return;
  sourceFile = null;
  invalidateResult();
  if (!file) {
    render();
    return;
  }
  if (file.size > MAX_INPUT_BYTES) {
    showError(copy[language].tooLarge);
    return;
  }
  if (file.size === 0) {
    showError(copy[language].emptyFile);
    return;
  }
  sourceFile = file;
  phase = 'selected';
  render();
}

fileInput.addEventListener('change', () => {
  const file = fileInput.files?.[0];
  chooseFile(file);
  fileInput.value = '';
});

slotSelect.addEventListener('change', () => {
  if (phase === 'busy') return;
  invalidateResult();
  render();
});

for (const [code, button] of Object.entries(languageButtons)) {
  button.addEventListener('click', () => {
    language = code;
    render();
  });
}

dropzone.addEventListener('dragover', (event) => {
  event.preventDefault();
  if (phase === 'busy') return;
  event.dataTransfer.dropEffect = 'copy';
  dropzone.classList.add('is-dragging');
});

dropzone.addEventListener('dragleave', (event) => {
  if (!dropzone.contains(event.relatedTarget)) {
    dropzone.classList.remove('is-dragging');
  }
});

dropzone.addEventListener('drop', (event) => {
  event.preventDefault();
  dropzone.classList.remove('is-dragging');
  if (phase === 'busy') return;
  const files = event.dataTransfer.files;
  if (files.length !== 1) {
    sourceFile = null;
    invalidateResult();
    showError(copy[language].multipleFiles);
    return;
  }
  chooseFile(files[0]);
});

for (const eventName of ['dragover', 'drop']) {
  window.addEventListener(eventName, (event) => {
    if ([...event.dataTransfer.types].includes('Files')) event.preventDefault();
  });
}

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (!sourceFile || phase === 'busy') return;
  invalidateResult();
  const run = generation;
  phase = 'busy';
  render();
  try {
    const input = new Uint8Array(await sourceFile.arrayBuffer());
    if (input.byteLength > MAX_INPUT_BYTES) throw new Error(copy[language].tooLarge);
    const converted = await convertSave(input, sourceFile.name, { slot: slotSelect.value });
    if (run !== generation) return;
    output = converted;
    phase = 'success';
    render();
    downloadButton.focus();
  } catch (error) {
    if (run !== generation) return;
    showError(error instanceof Error ? error.message : copy[language].unknownError);
  }
});

downloadButton.addEventListener('click', () => {
  if (!output || phase === 'busy') return;
  const url = URL.createObjectURL(new Blob([output.zip], { type: 'application/zip' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = output.downloadName.split(/[\\/]/).pop() || 'lost-odyssey-save.zip';
  document.body.append(link);
  link.click();
  link.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 10_000);
  phase = 'downloaded';
  render();
});

render();
