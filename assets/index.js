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

// 模糊搜索函数
function fuzzySearch(pattern, str) {
    pattern = pattern.toLowerCase();
    str = str.toLowerCase();
    let patternIdx = 0;
    let strIdx = 0;
    while (patternIdx < pattern.length && strIdx < str.length) {
        if (pattern[patternIdx] === str[strIdx]) {
            patternIdx++;
        }
        strIdx++;
    }
    return patternIdx === pattern.length;
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

    function updateAutocomplete(value) {
        if (!value) {
            autocompleteList.style.display = 'none';
            matches = [];
            activeIndex = -1;
            return;
        }

        const uniqueCalls = [...new Set(functionCalls)];
        matches = uniqueCalls
            .filter(call => fuzzySearch(value.toLowerCase(), call.toLowerCase()))
            .slice(0, 5);

        if (matches.length > 0) {
            autocompleteList.innerHTML = matches.map((match, index) => `<div class="autocomplete-item${index === activeIndex ? ' active' : ''}">${match}</div>`).join('');
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
            mp.innerHTML = '<h3>' + STR.markedFunctions + '</h3><p class="no-marks">' + STR.noMarks + '</p>';
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
    markedPanel.innerHTML = '<h3>' + STR.markedFunctions + '</h3>';
    treeContainer.appendChild(markedPanel);

    treeParent.appendChild(treeContainer);
}

// 更新标签面板
function updateTagsPanel() {
    const markedPanel = document.querySelector('.marked-panel');
    const markedNodes = document.querySelectorAll('.marked');

    let markedContent = `<h3>${STR.markedFunctions}</h3>`;

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
        markedContent += `<p class="no-marks">${STR.noMarks}</p>`;
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

// 初始化
document.addEventListener('DOMContentLoaded', () => {
    createControls();
    createSearchBox();
    createTagsPanel();
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