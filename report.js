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

// $(function() {
//   $('#fontSize')
//     .bind('change', function(){ setFontSize($(this)); })
//     .bind('keyup', function(e){
//       if (e.keyCode == 27) {
//         $(this).val('1');
//         $('body').css({ fontSize: '1em' });  
//       } else {
//         setFontSize($(this));
//       }
//     });

//   $(window)
//     .bind('keyup', function(e){
//       if (e.keyCode == 27) {
//         $('#fontSize').val('1');
//         $('body').css({ fontSize: '1em' });  
//       }
//     });

// });

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
    const controls = document.createElement('div');
    controls.className = 'controls';

    controls.innerHTML = `
        <div class="controls-left">
            <button class="search-button" onclick="expandAll()">展开全部</button>
            <button class="search-button" onclick="collapseAll()">折叠全部</button>
        </div>
        <div class="controls-right">
            <button class="help-button" onclick="showHelp()">
                <svg width="16" height="16" viewBox="0 0 16 16">
                    <path fill="currentColor" d="M8 0C3.6 0 0 3.6 0 8s3.6 8 8 8 8-3.6 8-8-3.6-8-8-8zm0 14c-3.3 0-6-2.7-6-6s2.7-6 6-6 6 2.7 6 6-2.7 6-6 6z"/>
                    <path fill="currentColor" d="M8 5.3c-.7 0-1.3.6-1.3 1.3 0 .3.2.5.5.5s.5-.2.5-.5c0-.2.1-.3.3-.3s.3.1.3.3c0 .2-.1.3-.3.3-.3 0-.5.2-.5.5v.8c0 .3.2.5.5.5s.5-.2.5-.5v-.3c.6-.2 1-.8 1-1.5 0-.7-.6-1.3-1.3-1.3zM8 10c-.3 0-.5.2-.5.5s.2.5.5.5.5-.2.5-.5-.2-.5-.5-.5z"/>
                </svg>
                帮助
            </button>
        </div>
    `;

    const tree = document.querySelector('.tree');
    tree.parentNode.insertBefore(controls, tree);
}

// 搜索功能增强
function createSearchBox() {
    const searchContainer = document.createElement('div');
    searchContainer.className = 'search-container';

    const searchBox = document.createElement('div');
    searchBox.className = 'search-box';
    searchBox.innerHTML = `
        <div class="search-input-container">
            <input type="text" class="search-input" placeholder="输入搜索关键词...">
            <div class="autocomplete-items" style="display: none;"></div>
        </div>
        <button class="search-button">搜索</button>
        <button class="search-button" onclick="clearHighlight()">清除</button>
    `;

    searchContainer.appendChild(searchBox);

    // 将搜索框插入到控制栏之后
    const controls = document.querySelector('.controls');
    controls.parentNode.insertBefore(searchContainer, controls.nextSibling);

    const input = searchBox.querySelector('.search-input');
    const searchBtn = searchBox.querySelector('.search-button');
    const autocompleteList = searchBox.querySelector('.autocomplete-items');
    let activeIndex = -1;
    let matches = [];

    // 自动完成功能
    function updateAutocomplete(value) {
        if (!value) {
            autocompleteList.style.display = 'none';
            matches = [];
            activeIndex = -1;
            return;
        }

        matches = functionCalls.filter(call =>
            fuzzySearch(value.toLowerCase(), call.toLowerCase())
        ).slice(0, 5);

        if (matches.length > 0) {
            autocompleteList.innerHTML = matches
                .map((match, index) => `
                    <div class="autocomplete-item${index === activeIndex ? ' active' : ''}">${match}</div>
                `)
                .join('');
            autocompleteList.style.display = 'block';
        } else {
            autocompleteList.style.display = 'none';
        }
    }

    // 选择建议项
    function selectItem(index) {
        if (index >= 0 && index < matches.length) {
            input.value = matches[index];
            autocompleteList.style.display = 'none';
            activeIndex = -1;
            matches = [];
            input.focus();
        }
    }

    // 键盘导航
    input.addEventListener('keydown', (e) => {
        const items = autocompleteList.querySelectorAll('.autocomplete-item');

        switch (e.key) {
            case 'ArrowDown':
                e.preventDefault();
                if (autocompleteList.style.display === 'none') {
                    updateAutocomplete(input.value);
                }
                activeIndex = Math.min(activeIndex + 1, matches.length - 1);
                break;

            case 'ArrowUp':
                e.preventDefault();
                activeIndex = Math.max(activeIndex - 1, -1);
                break;

            case 'Tab':
                if (matches.length > 0) {
                    e.preventDefault();
                    if (activeIndex === -1) {
                        selectItem(0);
                    } else {
                        selectItem(activeIndex);
                    }
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

        // 更新高亮状态
        items.forEach((item, index) => {
            item.classList.toggle('active', index === activeIndex);
        });
    });

    // 输入时更新建议
    input.addEventListener('input', (e) => {
        activeIndex = -1;
        updateAutocomplete(e.target.value);
    });

    // 点击建议项
    autocompleteList.addEventListener('click', (e) => {
        if (e.target.classList.contains('autocomplete-item')) {
            input.value = e.target.textContent;
            autocompleteList.style.display = 'none';
            performSearch(input.value);
        }
    });

    // 搜索功能
    const performSearch = (searchTerm) => {
        if (!searchTerm) return;

        clearHighlight();
        let found = false;

        labels.forEach((label, index) => {
            if (fuzzySearch(searchTerm.toLowerCase(), functionCalls[index].toLowerCase())) {
                expandAndHighlight(label);
                found = true;
            }
        });

        if (!found) {
            alert('未找到匹配内容');
        }
    };

    // 绑定搜索按钮事件
    searchBtn.addEventListener('click', () => performSearch(input.value));

    // 点击其他地方时关闭建议列表
    document.addEventListener('click', (e) => {
        if (!searchBox.contains(e.target)) {
            autocompleteList.style.display = 'none';
            activeIndex = -1;
            matches = [];
        }
    });
}

// 创建标签面板
function createTagsPanel() {
    // 创建容器包装 tree 和标记面板
    const treeContainer = document.createElement('div');
    treeContainer.className = 'tree-container';

    // 移动现有的 tree 到新容器
    const tree = document.querySelector('.tree');
    const treeParent = tree.parentNode;
    treeContainer.appendChild(tree);

    // 创建标记面板
    const markedPanel = document.createElement('div');
    markedPanel.className = 'marked-panel';
    markedPanel.innerHTML = '<h3>已标记函数</h3>';
    treeContainer.appendChild(markedPanel);

    // 将容器插入到原来 tree 的位置
    treeParent.appendChild(treeContainer);
}

// 更新标签面板
function updateTagsPanel() {
    const markedPanel = document.querySelector('.marked-panel');
    const markedNodes = document.querySelectorAll('.marked');

    let markedContent = '<h3>已标记函数</h3>';

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
        markedContent += '<p class="no-marks">暂无标记</p>';
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

// 创建帮助模态框
function createHelpModal() {
    const modal = document.createElement('div');
    modal.className = 'modal';
    modal.id = 'helpModal';

    modal.innerHTML = `
        <div class="modal-content">
            <span class="modal-close" onclick="hideHelp()">&times;</span>
            <h2>使用帮助</h2>
            <div class="modal-body">
                <h3>快捷键</h3>
                <ul class="shortcut-list">
                    <li class="shortcut-item">
                        <span>搜索</span>
                        <span class="shortcut-key">Ctrl + F</span>
                    </li>
                    <li class="shortcut-item">
                        <span>展开全部</span>
                        <span class="shortcut-key">Ctrl + E</span>
                    </li>
                    <li class="shortcut-item">
                        <span>折叠全部</span>
                        <span class="shortcut-key">Ctrl + C</span>
                    </li>
                </ul>
                
                <h3>功能说明</h3>
                <ul>
                    <li>右键点击函数名可以标记/取消标记</li>
                    <li>标记的函数会在右侧面板显示，点击可快速定位</li>
                    <li>搜索支持模糊匹配和自动补全</li>
                    <li>颜色说明：
                        <ul>
                            <li>红色：执行时间占比 > 10%</li>
                            <li>绿色：执行时间占比 ≤ 10%</li>
                        </ul>
                    </li>
                </ul>
            </div>
        </div>
    `;

    document.body.appendChild(modal);
}

function showHelp() {
    document.getElementById('helpModal').style.display = 'block';
}

function hideHelp() {
    document.getElementById('helpModal').style.display = 'none';
}

// 性能指标颜色处理
function addPerformanceColors() {
    labels.forEach(label => {
        const text = label.textContent;
        const percentage = parseFloat(text.match(/(\d+\.\d+)%/)?.[1] || 0);

        if (percentage > 10) {
            label.style.color = '#dc3545'; // 红色，执行时间较长
        } else {
            label.style.color = '#28a745'; // 绿色，执行时间较短
        }
    });
}

// 添加工具提示
function addTooltips() {
    labels.forEach(label => {
        const text = label.textContent;
        const percentage = text.match(/(\d+\.\d+)%/)?.[1] || '0';
        const calls = text.match(/(\d+)\/(\d+)/)?.[0] || '';

        label.classList.add('tooltip');
        const tooltip = document.createElement('span');
        tooltip.className = 'tooltiptext';
        tooltip.textContent = `执行时间占比: ${percentage}% | 调用次数: ${calls}`;
        label.appendChild(tooltip);
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

// 确保默认折叠状态
function ensureCollapsed() {
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
    createHelpModal();
    addPerformanceColors();
    addTooltips();
    // 确保初始状态为折叠
    ensureCollapsed();

    // 点击空白处关闭帮助模态框
    window.addEventListener('click', (e) => {
        const modal = document.getElementById('helpModal');
        if (e.target === modal) {
            hideHelp();
        }
    });
});

// 快捷键支持
document.addEventListener('keydown', function (event) {
    // Ctrl + F: 搜索
    if (event.ctrlKey && event.key === 'f') {
        event.preventDefault();
        createSearchBox();
    }
    // Ctrl + E: 展开全部
    else if (event.ctrlKey && event.key === 'e') {
        event.preventDefault();
        expandAll();
    }
    // Ctrl + C: 折叠全部
    else if (event.ctrlKey && event.key === 'c') {
        event.preventDefault();
        collapseAll();
    }
});