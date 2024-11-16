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

let labels = document.querySelectorAll('.tree_label');

// 遍历这些元素并获取它们的内容
let contentArray = Array.from(labels).map(label => label.textContent);
let functioncalls = contentArray.map(item => {
    let match = item.match(/^[^(]+/);
    return match ? match[0].trim() : "";
});

// 自定义搜索函数
function customSearch() {

    // 清除上一次搜索的高亮
    clearHighlight();

    let searchTerm = prompt("search keywords: "); // 获取用户输入的搜索词
    if (!searchTerm) return;

    let results = [];
    labels.forEach((label, index) => {
        if (functioncalls[index].includes(searchTerm)) {
            results.push(label);
        }
    });

    if (results.length > 0) {
        // console.log("找到以下匹配项:");
        results.forEach(label => {
            // 展开并高亮匹配的元素
            expandAndHighlight(label);
            console.log(label.textContent);
        });
    } else {
        // console.log("未找到匹配内容");
        // tell user no match found
        
    }
}

// 展开并高亮元素的函数
// 展开并高亮元素的函数
function expandAndHighlight(label) {
    // 向上遍历并展开所有父节点
    let parent = label;
    while (parent) {
        if (parent.tagName === 'LI' || parent.tagName === 'UL') {
            // 找到与该项相关联的复选框元素
            let input = parent.querySelector('input[type="checkbox"]');
            if (input) {
                input.checked = true; // 展开当前折叠的节点
            }
        }
        parent = parent.parentElement; // 继续向上遍历
    }

    // 高亮当前元素
    label.style.backgroundColor = "yellow"; // 设置高亮颜色
    // setTimeout(() => label.style.backgroundColor = "", 2000); // 2秒后恢复原样
}


// 清除所有高亮
function clearHighlight() {
    labels.forEach(label => {
        label.style.backgroundColor = ""; // 恢复为默认颜色
    });
}

document.addEventListener('keydown', function (event) {
    if (event.ctrlKey && event.key === 'f') {
        event.preventDefault(); // 阻止默认的搜索行为
        customSearch(); // 调用自定义搜索函数
    }
});

// 为每个 label 元素添加右键点击事件监听
// 为每个 label 元素添加右键点击事件监听
labels.forEach(label => {
    // 初始化一个变量,用于追踪当前是否已变蓝
    let isColored = false;

    label.addEventListener('contextmenu', function(event) {
        event.preventDefault(); // 阻止默认的右键菜单显示

        // 切换背景颜色和状态
        if (isColored) {
            label.style.backgroundColor = ''; // 取消蓝色
        } else {
            label.style.backgroundColor = '#ff5959';
        }

        // 切换 isBlue 的状态
        isColored = !isColored;
    });
});