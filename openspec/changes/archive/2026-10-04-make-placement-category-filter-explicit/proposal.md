## Why

放置目录用显示文字 All 和空字符串代表不限制分类，混淆展示与语义。分类保持开放字符串身份，使用显式无过滤状态即可去除保留文字和 GUI 耦合。

## What Changes

- Scene 查询以可选分类身份表达全部或指定分类。
- Editor 将 All 作为显示标签，显式保存无过滤状态，并区分同名真实分类的控件身份。
- 保持搜索、顺序、多分类去重、目录及放置操作；验证真实分类名 All 不再充当通配符。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `object-placement`: 全部过滤与分类字符串身份分离。

## Impact

Scene ObjectPlacement 查询的内部 C++ 调用方、Editor palette 和测试。已有分类 ID、对象 ID、automation schema 和持久化保持不变；不新增分类枚举。
