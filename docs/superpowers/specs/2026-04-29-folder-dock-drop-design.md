# 拖拽文件夹到任务栏创建插件 — 设计文档

> 日期: 2026-04-29
> 状态: 待评估
> 分支: fix/BUG-354203

## 需求概述

用户将外部文件夹拖拽到任务栏（Dock）上时，动态创建一个"文件夹浏览器"插件实例。该实例固定在任务栏上，点击后弹出面板可浏览文件夹内容。支持持久化（重启后保留）和右键菜单移除。

## 方案 A（推荐）: FolderDockManager 管理器插件

### 核心思路

创建一个名为 `FolderDockManager` 的新 dock 插件。该插件本身注册到 dock 中，内部管理多个"文件夹快捷入口"子项。用户拖拽文件夹到 dock 上时，由该插件捕获 drop 事件，在内部创建新的子项并持久化。

### 架构设计

```
┌─────────────────── Dock ──────────────────────┐
│ Left (0-10) │ Center (10-20) │ Right (20-30)  │
│             │   TaskManager   │    Tray        │
│             │   [FolderMgr]  │  ShowDesktop   │
│             │    ├─ /home    │                │
│             │    ├─ /docs    │                │
│             │    └─ ...      │                │
└───────────────────────────────────────────────┘
```

### 组件设计

#### 1. FolderDockManager 插件 (C++)

**文件结构:**
```
panels/dock/folderdockmanager/
├── CMakeLists.txt
├── folderdockmanager.h        # 主插件类
├── folderdockmanager.cpp
├── foldermodel.h              # 文件夹内容模型
├── foldermodel.cpp
├── folderlistmodel.h          # 管理文件夹列表
├── folderlistmodel.cpp
├── package/
│   ├── folderdockmanager.qml  # 主 QML（管理多个文件夹入口）
│   ├── folderitem.qml         # 单个文件夹入口组件
│   ├── folderbrowser.qml      # 文件夹浏览弹出面板
│   └── metadata.json
└── translations/
```

**C++ 类:**

```cpp
// folderdockmanager.h
class FolderDockManager : public DApplet {
    Q_OBJECT
    // 暴露给 QML 的属性
    Q_PROPERTY(QAbstractItemModel* folderListModel READ folderListModel CONSTANT)
public:
    explicit FolderDockManager(QObject* parent = nullptr);
    void init() override;

    // QML 调用：添加文件夹
    Q_INVOKABLE void addFolder(const QString& path);
    // QML 调用：移除文件夹
    Q_INVOKABLE void removeFolder(const QString& path);
    // QML 调用：重排文件夹
    Q_INVOKABLE void reorderFolders(const QStringList& paths);
};
```

```cpp
// folderlistmodel.h — 管理已固定的文件夹列表
class FolderListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { FolderPathRole = Qt::UserRole + 1, FolderNameRole, FolderIconRole };
    // 持久化到 DConfig
    void loadFromConfig();
    void saveToConfig();
    // 增删操作
    void addFolder(const QString& path);
    void removeFolder(int index);
};
```

```cpp
// foldermodel.h — 单个文件夹的内容浏览模型（复用 demoplugin 的 DirectoryModel 思路）
class FolderModel : public QAbstractListModel {
    Q_OBJECT
public:
    void setPath(const QString& path);
    // 返回文件/文件夹列表
};
```

**metadata.json:**
```json
{
    "Plugin": {
        "Version": "1.0",
        "Id": "org.deepin.ds.dock.folderdockmanager",
        "Url": "folderdockmanager.qml",
        "Parent": "org.deepin.ds.dock"
    }
}
```

#### 2. QML 层设计

**folderdockmanager.qml（主组件）:**

- 在 dock 上显示为可扩展的区域，包含多个文件夹入口图标
- `dockOrder: 17`（与 demoplugin 同区域，位于 TaskManager 旁）
- `shouldVisible: true`（当有至少一个文件夹时可见）
- 外层 DropArea 捕获外部文件夹拖入事件

**folderitem.qml（单个文件夹入口）:**

- 显示文件夹图标 + 文件夹名（tooltip）
- 点击弹出 folderbrowser.qml
- 支持右键菜单（移除、在文件管理器中打开）
- 支持 dock 内拖拽排序

**folderbrowser.qml（文件夹浏览面板）:**

- 复用 demoplugin 的文件浏览 UI 设计
- Popup 形式弹出
- 支持列表/网格视图切换
- 支持点击文件用默认应用打开

#### 3. 持久化

使用 DConfig 保存文件夹列表：

```cpp
// DConfig 配置
DConfig::create("org.deepin.dde.shell", "org.deepin.ds.dock.folderdockmanager")
```

存储内容：
```json
{
    "pinnedFolders": [
        { "path": "/home/user/Documents", "order": 0 },
        { "path": "/home/user/Downloads", "order": 1 }
    ]
}
```

#### 4. DropArea 设计

```qml
// folderdockmanager.qml 中的 DropArea
DropArea {
    id: globalDropArea
    anchors.fill: parent
    keys: ["text/uri-list"]

    onDropped: (drop) => {
        if (drop.hasUrls) {
            for (var url of drop.urls) {
                var path = UrlUtils.toLocalFile(url)
                if (FolderModel.isDir(path)) {
                    FolderDockManager.addFolder(path)
                }
            }
        }
    }
}
```

### 数据流

```
用户拖拽文件夹 → DropArea.onDropped → FolderDockManager.addFolder(path)
    → FolderListModel.addFolder(path)
        → DConfig 持久化
        → QML ListView 更新，显示新图标
```

### 优点

1. **完全兼容现有架构** — 不需要修改 dock 核心代码
2. **资源开销小** — 所有文件夹入口共享一个插件进程
3. **易于管理** — 统一的添加/删除/排序接口
4. **可扩展** — 未来可以支持更多类型的外部拖入（文件、应用等）

### 缺点与风险

1. 如果某个文件夹浏览触发异常，可能影响所有入口
2. dock 空间有限，多个文件夹入口可能溢出
3. 文件夹重命名/删除后需要处理失效场景

---

## 方案 B: 扩展 Dock 核心，动态加载 demoplug 实例

### 核心思路

修改 dock 的 `main.qml`，在全局添加 DropArea。当检测到文件夹拖入时，通过 `DPluginLoader` 动态创建新的 demoplug 插件实例，传入文件夹路径。

### 架构设计

```
┌─────────────────── Dock main.qml ─────────────┐
│  [全局 DropArea 捕获文件夹拖入]                   │
│  → DPluginLoader.load("demoplugin", {path})   │
│  → 新的 demoplug 实例出现在 dock 中              │
└───────────────────────────────────────────────┘
```

### 需要修改的文件

1. `panels/dock/package/main.qml` — 添加全局 DropArea
2. `panels/dock/demoplugin/demoplugin.h/cpp` — 支持接收初始路径参数
3. `panels/dock/demoplugin/demoplugin.qml` — 支持多实例模式（简化为单文件夹浏览）
4. 新增 DConfig 用于持久化文件夹列表

### 关键代码变更

```qml
// main.qml 新增全局 DropArea
DropArea {
    anchors.fill: parent
    keys: ["text/uri-list"]
    onDropped: (drop) => {
        // 检测是否为文件夹
        // 动态加载新的 demoplug 实例
    }
}
```

```cpp
// demoplugin.cpp 需要支持从 DConfig 读取初始路径
void Demoplugin::init() {
    QString folderPath = meta()->value("folderPath").toString();
    if (!folderPath.isEmpty()) {
        m_directoryModel->setPath(folderPath);
    }
}
```

### 优点

- 复用现有 demoplug 代码
- 每个文件夹独立实例，隔离性好

### 缺点

- **需要修改 dock 核心代码**（main.qml），增加维护风险
- demoplug 原本不是为多实例设计，需要大幅重构
- 每个实例可能是独立进程，资源开销较大
- 全局 DropArea 可能与现有 dock 交互冲突

### 评估建议

适合在以下场景考虑：
- 希望每个文件夹完全隔离（独立进程）
- demoplug 的文件夹浏览功能已经非常成熟，不想重写
- 团队有能力维护 dock 核心改动

---

## 方案 C: 独立 FolderLauncher 插件类型

### 核心思路

创建一种全新的轻量级插件类型 `FolderLauncher`，每个文件夹对应一个完全独立的插件实例。通过修改插件加载器支持运行时动态注册。

### 架构设计

```
┌─────────────────── Dock ──────────────────────┐
│  PluginLoader 运行时注册                        │
│  → FolderLauncher("/home/user/Documents")     │
│  → FolderLauncher("/home/user/Downloads")     │
│  每个实例独立进程、独立生命周期                    │
└───────────────────────────────────────────────┘
```

### 需要修改/创建的文件

1. **新增 FolderLauncher 插件** — 轻量级单文件夹浏览插件
2. **修改 DPluginLoader** — 支持运行时动态创建插件实例
3. **修改 DAppletFactory** — 支持带参数的实例创建
4. **修改 dock main.qml** — 添加全局 DropArea
5. **新增 DConfig** — 持久化文件夹列表
6. **修改 dock 启动逻辑** — 从 DConfig 读取并恢复 FolderLauncher 实例

### 关键变更

```cpp
// DPluginLoader 需要新增 API
class DPluginLoader {
public:
    // 现有：从 metadata.json 静态加载
    // 新增：运行时创建实例
    static DApplet* createInstance(const QString& pluginId,
                                   const QVariantMap& parameters);
};

// FolderLauncher 极简插件
class FolderLauncher : public DApplet {
    Q_OBJECT
    Q_PROPERTY(QString folderPath READ folderPath WRITE setFolderPath)
public:
    // 轻量级实现，只负责显示图标和弹出文件浏览
};
```

### 优点

- 完全隔离，每个文件夹独立进程
- 单个文件夹浏览出错不影响其他
- 语义清晰，一个文件夹 = 一个 dock 入口

### 缺点

- **资源开销最大** — 每个文件夹一个独立进程
- **需要修改插件加载器核心**（DPluginLoader、DAppletFactory）
- 实现复杂度最高，需要改动基础设施
- 运行时插件注册是全新的机制，需要充分测试稳定性

### 评估建议

适合在以下场景考虑：
- 对隔离性有极高要求（如安全场景）
- dock 需要支持多种类型的动态插件（不仅是文件夹）
- 团队计划长期投资插件基础设施的动态化

---

## 方案对比总结

| 维度 | 方案 A: FolderDockManager | 方案 B: 扩展 Dock 核心 | 方案 C: 独立 FolderLauncher |
|------|---------------------------|------------------------|------------------------------|
| **改动范围** | 新增插件，不改核心 | 改动 dock 核心 | 改动插件加载器 + dock 核心 |
| **资源开销** | 低（共享进程） | 中（多实例多进程） | 高（每文件夹独立进程） |
| **隔离性** | 共享进程，互相影响 | 中等 | 完全隔离 |
| **实现复杂度** | 低 | 中 | 高 |
| **维护风险** | 低 | 中 | 高 |
| **可扩展性** | 好（插件内部扩展） | 中 | 好（基础设施级扩展） |
| **推荐度** | **推荐** | 备选 | 暂不推荐 |

---

## 开放问题

以下问题需要在实施前确认：

1. **dock 空间溢出策略**：当文件夹入口过多导致 dock 溢出时，如何处理？
   - 选项 a: 隐藏溢出项，通过箭头滚动
   - 选项 b: 弹出面板显示完整列表
   - 选项 c: 限制最大数量

2. **文件夹失效处理**：文件夹被删除/重命名/移动后如何处理？
   - 选项 a: 图标变灰 + tooltip 提示失效
   - 选项 b: 自动移除
   - 选项 c: 保留但弹出警告

3. **拖拽区域**：是整个 dock 都接受拖拽，还是只有 dock 的空白区域？
   - 建议：空白区域 + 已有文件夹入口区域，避免与其他插件冲突

4. **国际化**：是否需要支持文件夹名称的翻译？
   - 建议：不需要，直接显示文件系统中的文件夹名
