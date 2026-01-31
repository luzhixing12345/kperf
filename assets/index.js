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
const FLAME_MIN_WIDTH = 0.4; // 最小可见宽度（占根节点百分比）
let currentZoomPath = [];

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

function percentToColor(pct) {
    const clamped = Math.max(0, Math.min(100, pct)) / 100;
    const start = { r: 0xfe, g: 0xe4, b: 0x36 }; // #fee436
    const end = { r: 0xd5, g: 0x27, b: 0x09 };   // #d52709
    const r = Math.round(start.r + (end.r - start.r) * clamped);
    const g = Math.round(start.g + (end.g - start.g) * clamped);
    const b = Math.round(start.b + (end.b - start.b) * clamped);
    return `rgb(${r}, ${g}, ${b})`;
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
    const totalHeight = (maxDepth + 1) * (FLAME_BAR_HEIGHT + FLAME_BAR_GAP) + FLAME_BAR_GAP;

    container.style.position = 'relative';
    container.style.height = totalHeight + 'px';

    bars.forEach(bar => {
        const el = document.createElement('div');
        el.className = 'flame-bar';
        const baseWidth = Math.max(bar.width, FLAME_MIN_WIDTH); // 避免极细不可见
        const widthPercent = baseWidth * FLAME_USABLE_WIDTH / 100;
        const leftPercent = FLAME_LEFT_PAD + bar.xStart * FLAME_USABLE_WIDTH / 100;
        el.style.left = `${leftPercent}%`;
        el.style.width = `${widthPercent}%`;
        const top = (maxDepth - bar.depth) * (FLAME_BAR_HEIGHT + FLAME_BAR_GAP);
        el.style.top = `${top}px`;
        el.style.height = `${FLAME_BAR_HEIGHT}px`;
        el.style.background = percentToColor(bar.percentage);
        el.title = `${bar.name} (${bar.percentage.toFixed(1)}% ${bar.count}/${bar.total})`;
        el.dataset.path = bar.path.join(',');

        const label = document.createElement('span');
        label.className = 'flame-label';
        label.textContent = `${bar.name} ${bar.percentage.toFixed(1)}%`;
        el.appendChild(label);

        el.addEventListener('click', () => {
            currentZoomPath = bar.path;
            renderFlameGraph();
        });

        container.appendChild(el);
    });
}

function showFlameView() {
    const treeContainer = document.getElementById('treeContainer');
    const flameContainer = document.getElementById('flameContainer');
    if (!treeContainer || !flameContainer) return;

    treeContainer.style.display = 'none';
    flameContainer.style.display = 'block';
    renderFlameGraph();
}

function showTreeView() {
    const treeContainer = document.getElementById('treeContainer');
    const flameContainer = document.getElementById('flameContainer');
    if (!treeContainer || !flameContainer) return;

    flameContainer.style.display = 'none';
    treeContainer.style.display = 'flex';
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
    if (active) active.style.display = '';

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

    function selectItem(index) {
        if (index >= 0 && index < matches.length) {
            input.value = matches[index];
            autocompleteList.style.display = 'none';
            activeIndex = -1;
            matches = [];
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
                    performSearch(input.value);
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
            performSearch(input.value);
        }
    });

    document.addEventListener('click', (e) => {
        if (!searchBox.contains(e.target)) {
            autocompleteList.style.display = 'none';
            activeIndex = -1;
            matches = [];
        }
    });

    const performSearch = (searchTerm) => {
        clearHighlight();
        if (!searchTerm) return;
        labels.forEach(label => {
            const text = label.textContent;
            const functionName = text.match(/^[^(]+/)?.[0]?.trim() || '';
            if (fuzzySearch(searchTerm.toLowerCase(), functionName.toLowerCase())) expandAndHighlight(label);
        });
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

// 创建标签面板
function createTagsPanel() {
    // If static container exists in HTML, use it; otherwise create and move tree
    const existing = document.querySelector('.tree-container');
    if (existing) {
        // ensure marked panel exists
        const markedPanel = existing.querySelector('.marked-panel');
        if (!markedPanel) {
            const mp = document.createElement('div');
            mp.className = 'marked-panel';
            mp.innerHTML = '<h3>' + 'Marked Functions' + '</h3><p class="no-marks">' + 'No marks' + '</p>';
            existing.appendChild(mp);
        }
        return;
    }

    // 创建容器包装 tree 和标记面板（回退至动态创建）
    const treeContainer = document.createElement('div');
    treeContainer.className = 'tree-container';

    const tree = document.querySelector('.tree');
    const treeParent = tree.parentNode;
    treeContainer.appendChild(tree);

    const markedPanel = document.createElement('div');
    markedPanel.className = 'marked-panel';
    markedPanel.innerHTML = '<h3>' + 'Marked Functions' + '</h3>';
    treeContainer.appendChild(markedPanel);

    treeParent.appendChild(treeContainer);
}

// 更新标签面板
function updateTagsPanel() {
    const markedPanel = document.querySelector('.marked-panel');
    const markedNodes = document.querySelectorAll('.marked');

    let markedContent = `<h3>Marked Functions</h3>`;

    if (markedNodes.length > 0) {
        markedContent += Array.from(markedNodes).map(node => {
            const functionName = node.textContent.split('(')[0].trim();
            const id = node.getAttribute('for');
            return `
                <div class="marked-item" data-id="${id}">
                    <span class="function-name">${functionName}</span>
                    <span class="remove">×</span>
                </div>
            `;
        }).join('');
    } else {
        markedContent += `<p class="no-marks">${'No marks'}</p>`;
    }

    markedPanel.innerHTML = markedContent;

    // 绑定标记项点击事件
    markedPanel.querySelectorAll('.marked-item').forEach(item => {
        item.addEventListener('click', (e) => {
            if (e.target.classList.contains('remove')) {
                const id = item.dataset.id;
                const label = document.querySelector(`label[for="${id}"]`);
                if (label) {
                    label.classList.remove('marked');
                    updateTagsPanel();
                }
            } else {
                const id = item.dataset.id;
                const label = document.querySelector(`label[for="${id}"]`);
                if (label) {
                    // 展开到该函数
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

                    // 移除其他元素的动画类
                    document.querySelectorAll('.highlight-pulse').forEach(el => {
                        el.classList.remove('highlight-pulse');
                    });

                    // 滚动到该函数位置并添加高亮动画
                    label.scrollIntoView({ behavior: 'smooth', block: 'center' });

                    // 等待滚动完成后添加动画
                    setTimeout(() => {
                        label.classList.add('highlight-pulse');
                        // 动画结束后移除类
                        label.addEventListener('animationend', () => {
                            label.classList.remove('highlight-pulse');
                        }, { once: true });
                    }, 200);
                }
            }
            e.stopPropagation();
        });
    });
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

        if (label.classList.contains('marked')) {
            label.classList.remove('marked');
        } else {
            label.classList.add('marked');
        }

        updateTagsPanel();
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
    createTagsPanel();
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
        if (!container) return;
        const isVisible = container.classList.contains('visible');
        if (!isVisible) {
            container.classList.add('visible');
            // focus the input when shown
            setTimeout(() => { input.focus(); }, 0);
        }
        const input = document.querySelector('.search-input');
        if (input) input.focus();
    }
});
