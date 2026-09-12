# Source v2 说明性示例

下例属于设计文档，不是已实现的加载能力或已生成的场景资产。它展示一个**没有模型依赖**的合法camera/light场景，供executor按design D8实现parser与P02/P04测试。实际shipped场景迁移必须保留原model内容与构图，不能用此例替换。

```json
{
  "type": "hyperion.scene",
  "schema_version": 2,
  "assets": [],
  "nodes": [
    {
      "id": "camera-rig",
      "name": "Camera rig",
      "parent": "",
      "translation": [0, 0, 0],
      "rotation": [0, 0, 0, 1],
      "scale": [1, 1, 1],
      "enabled": true
    },
    {
      "id": "camera-main",
      "name": "Main camera",
      "parent": "camera-rig",
      "translation": [0, 0, 8],
      "camera": {
        "verticalRadians": 0.729224966,
        "near": 0.01,
        "far": 1000,
        "focusDistance": 8
      }
    },
    {
      "id": "light-main",
      "name": "Main directional light",
      "directionalLight": {
        "color": [3, 2.85, 2.7],
        "intensity": 1,
        "castShadows": true
      }
    },
    {
      "id": "light-environment",
      "name": "Ambient light",
      "environmentLight": {
        "color": [0.22, 0.25, 0.3],
        "intensity": 1
      }
    }
  ],
  "defaultCamera": "camera-main",
  "mainDirectionalLight": "light-main",
  "environmentLight": "light-environment"
}
```

预期：4个nodes、1 camera、1 directional、1 environment、0 models/geometry groups/scene geometry draws（不含全屏或GUI绘制）。相机Eye=(0,0,8)、Forward=(0,0,-1)、pivot=(0,0,0)。该例方向光为identity姿态，因此surface-to-light=(0,0,1)；它**不是**旧场景兼容默认方向，兼容迁移必须按design D4构造相应旋转。没有模型时保持clear/GUI输出，不捏造几何满足benchmark readiness。

## 字段默认值和表示

- root `assets`、`nodes`为数组；三个选择字段可省略/空字符串表示未选择，不注入camera/light。v2不接受旧root `instances`/`camera`/`eye`/`target`作为第二权威数据源。
- node `id`必填且非空；`name`省略时用Id；`parent`省略/空表示根；`enabled`默认true。group没有payload。
- node有且仅有零个或一个`model`、`camera`、`directionalLight`、`environmentLight` payload。多个payload、未知payload/错误拼写和未支持的投影属性必须明确报错，不悄悄降级为group。
- 省略TRS表示identity，缺少单项时translation=(0,0,0)、rotation=(0,0,0,1)、scale=(1,1,1)。rotation=[x,y,z,w]是normalized quaternion。
- `transform`可替换TRS，使用16个column-major数。例如translation=(1,2,3)为`[1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1]`。存在`transform`时即使TRS是identity也禁止同时指定；矩阵必须finite affine。
- camera默认lens/focus为design D3的CPU默认；方向只从node transform来，不接受另一个`eye/target/forward`或`projection`字段覆盖。
- directional/environment `color`使用三个线性RGB浮点数，省略时默认white=(1,1,1)，`intensity`默认1；directional `castShadows`默认true。这些是新light payload的字段默认，**不是**场景自动默认灯；只有确实存在该node且被选中/启用才贡献光照。
- `model` payload必须指定asset ID（例如`{"asset":"model-a","visible":true}`），asset ID在root assets中映射为model source路径；model.visible默认true，与node.enabled不同。材质`material`、`surface`、`sectionSurfaces`为可选，使用对应现有CPU反射record的JSON值表示，由已有archive/material adapter解码；没有填写则继承导入model默认材质，不发明新的材质优先级。
- root assets v2仍为`{"id":"model-a","path":"../Models/Example.gltf"}`形式，路径相对source文件；反射source与native records使用既有完整FAssetRef表示，不将runtime Handle写成reference。
- plain v2 adapter须逐层校验root/node/camera/light允许字段以捕获拼写错误；反射记录继续使用其版本、migrations和validation机制。node顺序不要求parent先出现。

原生v4记录使用反射`type/version/fields`及FMat4/FVec3等记录编码，不要求它的字面JSON形状与plain source v2相同；两者最终解码为同一FSceneManifest语义。材料引用的VisitRecord和SaveAs重定位必须覆盖optional model payload内部结构。
