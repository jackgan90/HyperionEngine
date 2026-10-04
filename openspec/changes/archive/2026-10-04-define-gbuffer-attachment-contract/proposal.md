## Why

Renderer 的 GBuffer 角色散落在格式数组、contact shadow 槽位读取和 fullscreen/local-light semantic 数组中。用显式角色契约统一这些对应，可降低布局与消费者维护成本，同时保留现有渲染结果。

## What Changes

- Renderer 集中定义附件角色、物理槽位、绑定 semantic、默认格式和格式约束。
- 资源、格式校验、光照、debug 和 contact shadow 消费者按契约访问。
- 保持四槽位、GBuffer0..3、SV_Target0..3、shader 编码和缓存/退休行为。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `deferred-render-pipeline`: 添加由 Renderer 拥有的显式 GBuffer 附件映射要求。

## Impact

Renderer 布局及其直接消费者、契约与 GPU 测试、渲染文档。RasterOptions/Config 无 Renderer 依赖；无新渲染功能或资产/automation 协议迁移。
