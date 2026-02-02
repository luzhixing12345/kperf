function isNumber(n) {
    return !isNaN(parseFloat(n)) && isFinite(n);
}

function setFontSize(el) {
    var fontSize = el.val();

    if (isNumber(fontSize) && fontSize >= 0.5) {
        $('body').css({ fontSize: fontSize + 'em' });
    } else if (fontSize) {
        el.val('1');
        $('body').css({ fontSize: '1em' });
    }
}

// 获取所有树形节点标签
const labels = document.querySelectorAll('.tree_label');
const contentArray = Array.from(labels).map(label => label.textContent);
const functionCalls = contentArray.map(item => {
    let match = item.match(/^[^(]+/);
    return match ? match[0].trim() : "";
});

// 火焰图常量
const FLAME_BAR_HEIGHT = 26;
const FLAME_BAR_GAP = 2;
const FLAME_LEFT_PAD = 5;   // 预留左右空间（百分比）
const FLAME_USABLE_WIDTH = 90;
const FLAME_MIN_WIDTH = 0.1; // 最小可见宽度（占根节点百分比）
let currentZoomPath = [];
let markedPath = null; // 当前唯一标记的路径（与 tree / flame 同步）

// 解析 tree 生成火焰图数据
function parseTreeNode(li) {
    const label = li.querySelector(':scope > label.tree_label');
    if (!label) return null;

    const text = label.textContent || '';
    const name = (text.match(/^[^(]+/) || [''])[0].trim();
    const match = text.match(/\(([\d.]+)%\s+(\d+)\/(\d+)\)/);
    const percentage = match ? parseFloat(match[1]) : 0;
    const count = match ? parseInt(match[2], 10) : 0;
    const total = match ? parseInt(match[3], 10) : 0;

    const children = Array.from(li.querySelectorAll(':scope > ul > li'))
        .map(parseTreeNode)
        .filter(Boolean);

    return { name, percentage, count, total, children };
}

function parseTreeRoot() {
    const rootLi = document.querySelector('.tree > li');
    if (!rootLi) return null;
    return parseTreeNode(rootLi);
}

// --- FlameGraph-compatible color generation (mirrors flamegraph.pl) ---
const FLAME_COLOR_THEME = 'hot';   // same default as flamegraph.pl
const USE_HASH_BY_NAME = false;    // match default (hash only when --hash is used)
const paletteCache = new Map();    // keep function colors stable across renders

function nameHash(name) {
    // weighted hash that biases early characters; ported from flamegraph.pl namehash()
    let vector = 0;
    let weight = 1;
    let max = 1;
    let mod = 10;
    const trimmed = name.replace(/.(.*?)`/, '');
    for (let i = 0; i < trimmed.length && mod <= 12; i++) {
        const c = trimmed.charCodeAt(i) % mod;
        vector += (c / (mod - 1)) * weight;
        max += 1 * weight;
        weight *= 0.7;
        mod += 1;
    }
    return 1 - vector / max;
}

function sumNameHash(name) {
    let sum = 0;
    for (let i = 0; i < name.length; i++) {
        sum = (sum + name.charCodeAt(i)) >>> 0;
    }
    return sum;
}

function randomNameHash(name) {
    // deterministic pseudo-random seeded by function name
    let seed = sumNameHash(name) & 0x7fffffff;
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed / 0x80000000;
}

function colorForNameRaw(type, useHash, name) {
    let v1, v2, v3;
    if (useHash) {
        v1 = nameHash(name);
        v2 = v3 = nameHash(name.split('').reverse().join(''));
    } else {
        v1 = randomNameHash(name);
        v2 = randomNameHash(name);
        v3 = randomNameHash(name);
    }

    // theme palettes (hot/mem/io) and color palettes; defaults to "hot"
    if (type === 'hot') {
        const r = 205 + Math.floor(50 * v3);
        const g = 0 + Math.floor(230 * v1);
        const b = 0 + Math.floor(55 * v2);
        return { r, g, b };
    }
    if (type === 'mem') {
        const r = 0;
        const g = 190 + Math.floor(50 * v2);
        const b = 0 + Math.floor(210 * v1);
        return { r, g, b };
    }
    if (type === 'io') {
        const r = 80 + Math.floor(60 * v1);
        const g = r;
        const b = 190 + Math.floor(55 * v2);
        return { r, g, b };
    }
    if (type === 'red') {
        const r = 200 + Math.floor(55 * v1);
        const x = 50 + Math.floor(80 * v1);
        return { r, g: x, b: x };
    }
    if (type === 'green') {
        const g = 200 + Math.floor(55 * v1);
        const x = 50 + Math.floor(60 * v1);
        return { r: x, g, b: x };
    }
    if (type === 'blue') {
        const b = 205 + Math.floor(50 * v1);
        const x = 80 + Math.floor(60 * v1);
        return { r: x, g: x, b };
    }
    if (type === 'yellow') {
        const x = 175 + Math.floor(55 * v1);
        const b = 50 + Math.floor(20 * v1);
        return { r: x, g: x, b };
    }
    if (type === 'purple') {
        const x = 190 + Math.floor(65 * v1);
        const g = 80 + Math.floor(60 * v1);
        return { r: x, g, b: x };
    }
    if (type === 'aqua') {
        const r = 50 + Math.floor(60 * v1);
        const g = 165 + Math.floor(55 * v1);
        const b = 165 + Math.floor(55 * v1);
        return { r, g, b };
    }
    if (type === 'orange') {
        const r = 190 + Math.floor(65 * v1);
        const g = 90 + Math.floor(65 * v1);
        return { r, g, b: 0 };
    }

    // fallback: black
    return { r: 0, g: 0, b: 0 };
}

function colorForName(name, type = FLAME_COLOR_THEME) {
    if (!paletteCache.has(name)) {
        const rgb = colorForNameRaw(type, USE_HASH_BY_NAME, name);
        const color = `rgb(${rgb.r}, ${rgb.g}, ${rgb.b})`;
        paletteCache.set(name, { color });
    }
    return paletteCache.get(name);
}

function pushBar(node, depth, xStart, width, path, bars) {
    if (!node || width <= 0) return;
    const displayPct = node.total ? (node.count / node.total * 100) : node.percentage;
    bars.push({
        name: node.name,
        percentage: displayPct,
        count: node.count,
        total: node.total,
        depth,
        xStart,
        width,
        path: [...path]
    });
}

function buildDescBars(node, depth, xStart, width, path, bars) {
    if (!node || !node.children || node.children.length === 0 || node.count === 0) return;
    let offset = 0;
    node.children.forEach((child, idx) => {
        const ratio = child.count && node.count ? (child.count / node.count) : 0;
        const childWidth = width * ratio;
        if (childWidth <= 0) return;
        const childPath = [...path, idx];
        pushBar(child, depth, xStart + offset, childWidth, childPath, bars);
        buildDescBars(child, depth + 1, xStart + offset, childWidth, childPath, bars);
        offset += childWidth;
    });
}

function findNodeByPath(node, path) {
    let cur = node;
    for (const idx of path) {
        if (!cur.children || !cur.children[idx]) return null;
        cur = cur.children[idx];
    }
    return cur;
}

function getLabelByPath(path) {
    let li = document.querySelector('.tree > li');
    if (!li) return null;
    if (!path || path.length === 0) return li.querySelector(':scope > label.tree_label');
    for (const idx of path) {
        const children = Array.from(li.querySelectorAll(':scope > ul > li'));
        if (!children[idx]) return null;
        li = children[idx];
    }
    return li.querySelector(':scope > label.tree_label');
}

function getPathFromLi(li) {
    const path = [];
    let cur = li;
    while (cur) {
        const parentLi = cur.parentElement?.closest('li');
        if (!parentLi) break;
        const siblings = Array.from(parentLi.querySelectorAll(':scope > ul > li'));
        path.push(siblings.indexOf(cur));
        cur = parentLi;
    }
    return path.reverse();
}

function getPathFromLabel(label) {
    const li = label.closest('li');
    if (!li) return [];
    return getPathFromLi(li);
}

function pathToKey(path) {
    if (!path || path.length === 0) return '';
    return path.join(',');
}

function parsePathKey(key) {
    if (!key) return [];
    return key.split(',').filter(Boolean).map(n => parseInt(n, 10));
}

function clearAllMarks() {
    document.querySelectorAll('.marked').forEach(el => el.classList.remove('marked'));
}

function applyMarkedPath(path) {
    markedPath = path && path.length ? [...path] : [];
    const label = getLabelByPath(markedPath);
    if (label) label.classList.add('marked');
    const bar = document.querySelector(`.flame-bar[data-path="${pathToKey(markedPath)}"]`);
    if (bar) bar.classList.add('marked');
}

function renderFlameGraph() {
    const container = document.getElementById('flameContainer');
    if (!container) return;

    container.innerHTML = '';
    const data = parseTreeRoot();
    if (!data) {
        container.textContent = 'No data for flame graph';
        return;
    }

    // 1) 先放入根到焦点路径的所有节点，宽度都占满（方便点击返回）
    const bars = [];
    let node = data;
    let path = [];
    let depth = 0;
    pushBar(node, depth, 0, 100, path, bars);
    for (let i = 0; i < currentZoomPath.length; i++) {
        const idx = currentZoomPath[i];
        if (!node.children || !node.children[idx]) break;
        node = node.children[idx];
        path = [...path, idx];
        depth += 1;
        pushBar(node, depth, 0, 100, path, bars);
    }

    // 2) 在焦点节点之上展开子树，宽度按当前焦点重新计算
    buildDescBars(node, depth + 1, 0, 100, path, bars);

    const maxDepth = bars.reduce((max, b) => Math.max(max, b.depth), 0);
    const rows = maxDepth + 1;

    // Dynamically shrink bar height to fit the container and avoid inner scrollbars.
    let barHeight = FLAME_BAR_HEIGHT;
    let barGap = FLAME_BAR_GAP;

    const style = window.getComputedStyle(container);
    const padTop = parseFloat(style.paddingTop) || 0;
    const padBottom = parseFloat(style.paddingBottom) || 0;
    const containerHeight = container.clientHeight || container.getBoundingClientRect().height || 0;
    const availableHeight = Math.max(0, containerHeight - padTop - padBottom);

    const graphPadTop = 14;
    const graphPadBottom = 14;
    let drawableHeight = Math.max(0, availableHeight - graphPadTop - graphPadBottom);

    const MIN_BAR_HEIGHT = 4;
    const MIN_BAR_GAP = 1;
    let totalHeight = rows * (barHeight + barGap) + barGap;
    if (drawableHeight && totalHeight > drawableHeight) {
        const slot = drawableHeight / rows;
        barGap = Math.max(MIN_BAR_GAP, Math.floor(slot * 0.12));
        barHeight = Math.max(MIN_BAR_HEIGHT, Math.floor(slot - barGap));
        totalHeight = rows * (barHeight + barGap) + barGap;

        if (totalHeight > drawableHeight) {
            barGap = MIN_BAR_GAP;
            barHeight = Math.max(MIN_BAR_HEIGHT, Math.floor((drawableHeight - barGap) / rows - MIN_BAR_GAP));
            totalHeight = rows * (barHeight + barGap) + barGap;
        }
    }

    const graph = document.createElement('div');
    graph.className = 'flame-graph';
    graph.style.paddingTop = `${graphPadTop}px`;
    graph.style.paddingBottom = `${graphPadBottom}px`;

    const contentHeight = totalHeight + graphPadTop + graphPadBottom;
    graph.style.height = `${Math.max(contentHeight, 160)}px`;

    let scaleY = 1;
    if (availableHeight && contentHeight > availableHeight) {
        scaleY = availableHeight / contentHeight;
        graph.style.transformOrigin = 'top left';
        graph.style.transform = `scale(1, ${scaleY})`;
    } else {
        graph.style.transform = '';
    }
    container.appendChild(graph);

    bars.forEach(bar => {
        const el = document.createElement('div');
        el.className = 'flame-bar';
        el.style.position = 'absolute';
        const baseWidth = Math.max(bar.width, FLAME_MIN_WIDTH); // 避免极细不可见
        const widthPercent = baseWidth * FLAME_USABLE_WIDTH / 100;
        const leftPercent = FLAME_LEFT_PAD + bar.xStart * FLAME_USABLE_WIDTH / 100;
        el.style.left = `${leftPercent}%`;
        el.style.width = `${widthPercent}%`;
        const top = (maxDepth - bar.depth) * (barHeight + barGap);
        el.style.top = `${top}px`;
        el.style.height = `${barHeight}px`;
        el.style.lineHeight = `${barHeight}px`;
        const { color } = colorForName(bar.name);
        el.style.background = color;
        el.style.color = '#000';
        el.title = `${bar.name} (${bar.percentage.toFixed(1)}% ${bar.count}/${bar.total})`;
        el.dataset.path = bar.path.join(',');

        const label = document.createElement('span');
        label.className = 'flame-label';
        label.textContent = `${bar.name} ${bar.percentage.toFixed(1)}%`;
        el.appendChild(label);

        el.addEventListener('click', (e) => {
            // 如果用户在拖拽选择文本，不触发缩放/重绘，避免选区被清空
            if (window.getSelection && window.getSelection().toString()) return;
            currentZoomPath = bar.path;
            renderFlameGraph();
        });

        el.addEventListener('contextmenu', (event) => {
            event.preventDefault();
            // 选中文本时，右键仍允许菜单标记；清空选区以防浏览器弹出复制行为与标记冲突
            if (window.getSelection) {
                const sel = window.getSelection();
                if (sel && sel.toString()) sel.removeAllRanges();
            }
            const alreadyMarked = el.classList.contains('marked');
            clearAllMarks();
            if (!alreadyMarked) {
                applyMarkedPath(bar.path);
            } else {
                markedPath = [];
            }
        });

        graph.appendChild(el);
    });

    // 渲染后同步现有标记（若存在）
    if (markedPath !== null) {
        const bar = document.querySelector(`.flame-bar[data-path="${pathToKey(markedPath)}"]`);
        if (bar) bar.classList.add('marked');
    }
}

function showFlameView() {
    const treeContainer = document.getElementById('treeContainer');
    const flameContainer = document.getElementById('flameContainer');
    if (!treeContainer || !flameContainer) return;

    treeContainer.style.display = 'none';
    flameContainer.style.display = 'flex';
    renderFlameGraph();
}

function showTreeView() {
    const treeContainer = document.getElementById('treeContainer');
    const flameContainer = document.getElementById('flameContainer');
    if (!treeContainer || !flameContainer) return;

    flameContainer.style.display = 'none';
    treeContainer.style.display = 'flex';
    if (markedPath && markedPath.length) {
        const label = getLabelByPath(markedPath);
        if (label) {
            label.scrollIntoView({ behavior: 'smooth', block: 'center' });
            // 确保视觉高亮存在
            clearAllMarks();
            label.classList.add('marked');
        }
    }
}

// 模糊搜索函数 - 返回匹配分数和匹配位置
// https://github.com/bevacqua/fuzzysearch
// https://github.com/krisypal/fuse.js
// 
// - 每个匹配字符基础分 100 分
// - 连续匹配额外加分（递增）
// - 开头匹配加 50 分
// - 单词边界匹配加 30 分（驼峰/下划线）
// - 字符串越短分数越高
// - 匹配位置越靠前分数越高
function fuzzyMatch(pattern, str) {
    pattern = pattern.toLowerCase();
    str = str.toLowerCase();

    if (pattern.length === 0) return { score: 1, matches: [] };

    let patternIdx = 0;
    let strIdx = 0;
    let score = 0;
    let consecutiveBonus = 0;
    let matches = [];

    while (patternIdx < pattern.length && strIdx < str.length) {
        if (pattern[patternIdx] === str[strIdx]) {
            score += 100;
            consecutiveBonus += 10;
            score += consecutiveBonus;

            if (strIdx === 0) score += 50;

            if (strIdx === 0 || str[strIdx - 1] === '_' ||
                (strIdx > 0 && str[strIdx] >= 'a' && str[strIdx] <= 'z' && str[strIdx - 1] >= 'A' && str[strIdx - 1] <= 'Z')) {
                score += 30;
            }

            matches.push(strIdx);
            patternIdx++;
            strIdx++;
        } else {
            consecutiveBonus = 0;
            strIdx++;
        }
    }

    if (patternIdx < pattern.length) {
        return { score: 0, matches: [] };
    }

    score -= str.length * 2;

    if (matches.length > 0) {
        score -= matches[0] * 5;
    }

    return { score, matches };
}

function fuzzySearch(pattern, str) {
    const result = fuzzyMatch(pattern, str);
    return result.score > 0;
}

// 创建控制栏
function createControls() {
    // Bind tab buttons and help button
    const tabButtons = document.querySelectorAll('.tab-button');
    tabButtons.forEach(btn => {
        btn.addEventListener('click', (e) => {
            const tab = btn.getAttribute('data-tab');
            switchTab(tab);
        });
    });
}

// Switch visible tab content
function switchTab(name) {
    const contents = document.querySelectorAll('.tab-content');
    contents.forEach(c => c.style.display = 'none');

    const active = document.getElementById('tab-' + name);
    if (active) {
        active.style.display = '';
        if (name === 'ebpf') {
            renderEbpfTab();
        }
    }

    // update active class on tab buttons
    document.querySelectorAll('.tab-button').forEach(b => {
        b.classList.toggle('active', b.getAttribute('data-tab') === name);
    });
}

// 搜索功能增强
function createSearchBox() {
    // Use existing static search box if present and wire up behaviors
    const searchBox = document.querySelector('.search-box');
    if (!searchBox) return;

    const input = searchBox.querySelector('.search-input');
    const searchBtn = document.getElementById('searchBtn');
    const clearBtn = document.getElementById('clearBtn');
    const expandBtn = document.getElementById('expandBtn');
    const collapseBtn = document.getElementById('collapseBtn');
    const autocompleteList = searchBox.querySelector('.autocomplete-items');
    let activeIndex = -1;
    let matches = [];

    function highlightMatch(text, query) {
        let result = '';
        let textIdx = 0;
        let queryIdx = 0;
        const lowerText = text.toLowerCase();
        const lowerQuery = query.toLowerCase();

        while (textIdx < text.length && queryIdx < query.length) {
            if (lowerText[textIdx] === lowerQuery[queryIdx]) {
                result += `<strong>${text[textIdx]}</strong>`;
                queryIdx++;
            } else {
                result += text[textIdx];
            }
            textIdx++;
        }
        result += text.slice(textIdx);
        return result;
    }

    function updateAutocomplete(value) {
        if (!value) {
            autocompleteList.style.display = 'none';
            matches = [];
            activeIndex = -1;
            return;
        }

        const uniqueCalls = [...new Set(functionCalls)];
        const results = uniqueCalls
            .map(call => ({ call, ...fuzzyMatch(value.toLowerCase(), call.toLowerCase()) }))
            .filter(r => r.score > 0)
            .sort((a, b) => b.score - a.score)
            .slice(0, 5);

        matches = results.map(r => r.call);

        if (matches.length > 0) {
            autocompleteList.innerHTML = matches.map((match, index) => `<div class="autocomplete-item${index === activeIndex ? ' active' : ''}">${highlightMatch(match, value)}</div>`).join('');
            autocompleteList.style.display = 'block';
        } else {
            autocompleteList.style.display = 'none';
        }
    }

    let shouldExactMatch = false;

    function selectItem(index) {
        if (index >= 0 && index < matches.length) {
            input.value = matches[index];
            autocompleteList.style.display = 'none';
            activeIndex = -1;
            matches = [];
            shouldExactMatch = true;
            input.focus();
        }
    }

    input.addEventListener('keydown', (e) => {
        const items = autocompleteList.querySelectorAll('.autocomplete-item');
        switch (e.key) {
            case 'ArrowDown':
                e.preventDefault();
                if (autocompleteList.style.display === 'none') updateAutocomplete(input.value);
                activeIndex = Math.min(activeIndex + 1, matches.length - 1);
                break;
            case 'ArrowUp':
                e.preventDefault();
                activeIndex = Math.max(activeIndex - 1, -1);
                break;
            case 'Tab':
                if (matches.length > 0) {
                    e.preventDefault();
                    if (activeIndex === -1) selectItem(0); else selectItem(activeIndex);
                }
                break;
            case 'Enter':
                if (activeIndex !== -1) {
                    e.preventDefault();
                    selectItem(activeIndex);
                    performSearch(input.value, true);
                } else {
                    performSearch(input.value);
                }
                break;
            case 'Escape':
                autocompleteList.style.display = 'none';
                activeIndex = -1;
                matches = [];
                break;
        }
        items.forEach((item, index) => {
            item.classList.toggle('active', index === activeIndex);
        });
    });

    input.addEventListener('input', (e) => updateAutocomplete(e.target.value));

    autocompleteList.addEventListener('click', (e) => {
        if (e.target.classList.contains('autocomplete-item')) {
            const index = Array.from(autocompleteList.children).indexOf(e.target);
            selectItem(index);
            performSearch(input.value, true);
        }
    });

    document.addEventListener('click', (e) => {
        if (!searchBox.contains(e.target)) {
            autocompleteList.style.display = 'none';
            activeIndex = -1;
            matches = [];
        }
    });

    const performSearch = (searchTerm, forceExact = false) => {
        clearHighlight();
        if (!searchTerm) return;
        const searchLower = searchTerm.toLowerCase();
        const exact = forceExact || shouldExactMatch;
        shouldExactMatch = false;
        let firstMatch = null;
        labels.forEach(label => {
            const text = label.textContent;
            const functionName = text.match(/^[^(]+/)?.[0]?.trim() || '';
            const targetLower = functionName.toLowerCase();
            const matched = exact ? (targetLower === searchLower) : fuzzySearch(searchLower, targetLower);
            if (matched) expandAndHighlight(label);
            if (!firstMatch && label.classList.contains('highlight')) {
                firstMatch = label;
            }
        });
        if (firstMatch) {
            firstMatch.scrollIntoView({ behavior: 'smooth', block: 'center' });
        }
    };

    if (searchBtn) {
        // Toggle visibility of the search container when the search icon is clicked
        searchBtn.addEventListener('click', (e) => {
            const container = document.querySelector('.search-container');
            if (!container) return;
            const isVisible = container.classList.contains('visible');
            if (!isVisible) {
                container.classList.add('visible');
                // focus the input when shown
                setTimeout(() => { input.focus(); }, 0);
            } else {
                container.classList.remove('visible');
            }
        });
    }
    if (clearBtn) clearBtn.addEventListener('click', () => { clearHighlight(); input.value = ''; autocompleteList.style.display = 'none'; });
    if (expandBtn) expandBtn.addEventListener('click', () => { expandAll(); });
    if (collapseBtn) collapseBtn.addEventListener('click', () => { collapseAll(); });
    input.addEventListener('keypress', (e) => { if (e.key === 'Enter' && activeIndex === -1) performSearch(input.value); });

    const fireBtn = document.getElementById('fireBtn');
    const treeBtn = document.getElementById('treeBtn');
    let currentView = 'tree';

    if (fireBtn && treeBtn) {
        treeBtn.style.display = 'none';

        fireBtn.addEventListener('click', () => {
            if (currentView === 'fire') return;
            currentView = 'fire';
            fireBtn.style.display = 'none';
            treeBtn.style.display = 'inline-flex';
            showFlameView();
        });

        treeBtn.addEventListener('click', () => {
            if (currentView === 'tree') return;
            currentView = 'tree';
            treeBtn.style.display = 'none';
            fireBtn.style.display = 'inline-flex';
            showTreeView();
        });
    }
}

// 性能指标颜色处理
function addPerformanceColors() {
    labels.forEach(label => {
        const text = label.textContent;
        const match = text.match(/\(([\d.]+)%\s+(\d+)\/(\d+)\)/);

        if (match) {
            const percentage = parseFloat(match[1]);
            const count = parseInt(match[2], 10);
            const total = parseInt(match[3], 10);
            if (percentage < 5 || count == 1 || total < 5) {
                label.style.color = '#6c757d'; // 灰色，执行时间很短
                // 折叠
                input = label.parentElement.querySelector('input[type="checkbox"]');
                if (input) {
                    input.checked = false;
                }
            } else {
                label.style.color = '#09090b'; // 黑色，正常执行时间
            }
        } else {
            return;
        }
    });
}

// 展开并高亮元素
function expandAndHighlight(label) {
    // 向上遍历并展开所有父节点
    let parent = label.parentElement;
    while (parent) {
        if (parent.tagName === 'LI') {
            const input = parent.querySelector('input[type="checkbox"]');
            if (input) {
                input.checked = true;
            }
        }
        parent = parent.parentElement;
    }

    // 添加高亮效果
    label.classList.add('highlight');
}

// 清除高亮
function clearHighlight() {
    labels.forEach(label => {
        label.classList.remove('highlight');
    });
}

// 展开全部
function expandAll() {
    document.querySelectorAll('.tree input[type="checkbox"]').forEach(input => {
        input.checked = true;
    });
}

// 折叠全部
function collapseAll() {
    document.querySelectorAll('.tree input[type="checkbox"]').forEach(input => {
        input.checked = false;
    });
}

// 标记功能（右键点击）
labels.forEach(label => {
    label.addEventListener('contextmenu', function (event) {
        event.preventDefault();

        const alreadyMarked = label.classList.contains('marked');
        // 先清除所有标记，保证全局最多一个
        clearAllMarks();
        if (!alreadyMarked) {
            const path = getPathFromLabel(label);
            applyMarkedPath(path);
        } else {
            markedPath = [];
        }
    });
});

// 创建 tooltip 元素
function createTooltip() {
    const tooltip = document.createElement('div');
    tooltip.className = 'file-info-tooltip';
    document.body.appendChild(tooltip);
    return tooltip;
}

// 鼠标悬浮显示文件信息
function setupFileInfoTooltip() {
    const tooltip = createTooltip();
    const hoverTimers = new WeakMap();
    let hideTimer = null;
    let isTooltipHovered = false;

    function hideTooltip() {
        tooltip.style.display = 'none';
        isTooltipHovered = false;
    }

    function scheduleHide() {
        if (hideTimer) clearTimeout(hideTimer);
        hideTimer = setTimeout(() => {
            if (!isTooltipHovered) {
                hideTooltip();
            }
        }, 300);
    }

    labels.forEach(label => {
        label.addEventListener('mouseenter', (e) => {
            const fileInfo = label.getAttribute('text');
            if (!fileInfo || fileInfo.includes('?')) return;

            const timer = setTimeout(() => {
                tooltip.textContent = fileInfo;
                tooltip.style.display = 'block';
                const rect = label.getBoundingClientRect();
                tooltip.style.left = rect.left + 'px';
                tooltip.style.top = (rect.bottom + 5) + 'px';
            }, 1000);

            hoverTimers.set(label, timer);
        });

        label.addEventListener('mouseleave', () => {
            const timer = hoverTimers.get(label);
            if (timer) {
                clearTimeout(timer);
                hoverTimers.delete(label);
            }
            scheduleHide();
        });
    });

    // Tooltip 本身的鼠标事件
    tooltip.addEventListener('mouseenter', () => {
        isTooltipHovered = true;
        if (hideTimer) {
            clearTimeout(hideTimer);
            hideTimer = null;
        }
    });

    tooltip.addEventListener('mouseleave', () => {
        isTooltipHovered = false;
        hideTooltip();
    });
}

// 初始化
document.addEventListener('DOMContentLoaded', () => {
    createControls();
    createSearchBox();
    setupFileInfoTooltip();
    expandAll();
    addPerformanceColors(); // 对频率较低的函数标记为灰色并折叠
});

// 快捷键支持
document.addEventListener('keydown', function (event) {
    // Ctrl + F: 搜索
    if (event.ctrlKey && event.key === 'f') {
        event.preventDefault();
        const container = document.querySelector('.search-container');
        const input = document.querySelector('.search-input');
        if (!container) return;
        const isVisible = container.classList.contains('visible');
        if (!isVisible) {
            container.classList.add('visible');
            // 等待渲染后滚动并选中
            requestAnimationFrame(() => {
                if (input) {
                    input.focus();
                    if (input.value) input.select();
                }
            });
        } else {
            clearHighlight();
            container.scrollIntoView({ behavior: 'smooth', block: 'center' });
            if (input) {
                input.focus();
                if (input.value) input.select();
            }
        }
    }
});


const code = `int main() {
    return 0;
}
`;

const copyIcon = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="#9198a1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="opacity:1;"><rect width="14" height="14" x="8" y="8" rx="2" ry="2"/><path d="M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2"/></svg>';
const copiedIcon = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="#3fb950" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="opacity:1;"><path fill="none"     d="M20 6L9 17l-5-5"/></svg>';

async function writeClipboard(text) {
    if (navigator && navigator.clipboard && navigator.clipboard.writeText) {
        await navigator.clipboard.writeText(text);
        return true;
    }
    // fallback for older browsers
    try {
        const textarea = document.createElement('textarea');
        textarea.value = text;
        textarea.style.position = 'fixed';
        textarea.style.opacity = '0';
        document.body.appendChild(textarea);
        textarea.focus();
        textarea.select();
        const success = document.execCommand('copy');
        document.body.removeChild(textarea);
        return success;
    } catch (err) {
        return false;
    }
}

function createCodeBlock(code) {
    const wrapper = document.createElement('div');
    wrapper.className = 'code-block-wrapper';

    const pre = document.createElement('pre');
    pre.className = 'code-block';
    const codeEl = document.createElement('code');
    codeEl.textContent = code;
    pre.appendChild(codeEl);

    const copyBtn = document.createElement('div');
    copyBtn.className = 'copy-btn';
    copyBtn.innerHTML = copyIcon;
    copyBtn.setAttribute('aria-label', 'Copy code');

    copyBtn.addEventListener('click', async () => {
        const ok = await writeClipboard(codeEl.textContent || '');
        if (ok) {
            copyBtn.innerHTML = copiedIcon;
            setTimeout(() => { copyBtn.innerHTML = copyIcon; }, 1500);
        } else {
            console.error('Copy failed');
        }
    });

    wrapper.appendChild(pre);
    wrapper.appendChild(copyBtn);
    return wrapper;
}

let ebpfRendered = false;
function renderEbpfTab() {
    if (ebpfRendered) return;
    const container = document.getElementById('ebpfContainer');
    if (!container) return;

    container.innerHTML = '';
    const pre = createCodeBlock(code);
    container.appendChild(pre);
    ebpfRendered = true;
}
