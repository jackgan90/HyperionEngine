## Context

当前布局版本 1 固定为 BaseMetallic/Normals/Surface/Emissive 四附件。格式、绑定与 contact/local-light 消费者用下标表达角色，HLSL GBuffer 编码已集中于 Content/Shaders/Deferred/GBuffer.hlsli。

## Goals / Non-Goals

**Goals:** Renderer 单一描述表连接角色、槽位、semantic、默认格式和格式要求，消费者按角色读取。
**Non-Goals:** 不更改 shader 编码、目标顺序、布局版本、缓存算法、资源生命周期、RasterOptions 或 Config 依赖方向。

## Decisions

1. Renderer 公共 GBuffer 契约声明有限角色和显式描述，物理存储继续按旧槽位排列；角色查询不以 enum 序号隐式转换槽位。默认格式与验证从同表派生，规范格式规则保留。
2. FGBufferLayout 格式和能力校验归 Renderer；Formats 的物理数组保留以兼容布局比较与外部构造，提供按角色的访问。目标数组长度使用契约计数，目标创建与渲染绑定读取描述表；contact shadow 按 Normals/Surface 读取。
3. fullscreen/debug 使用同一角色到 EDeferredLightingSemantic 映射；local-light 明确消费 BaseMetallic、Normals、Surface，不通过前 3 个槽位推测依赖。
   单通道 R32Float/R8Unorm 原先可通过 Validate 但会在 BytesPerPixel 抛错；现在由布局校验明确拒绝，合法 RGBA 格式集合与输出不变。
4. GBuffer0..3 与 SV_Target0..3 是保留的 shader ABI。测试固定检查旧映射、格式/能力拒绝、默认/高精度字节成本，并复用 shader 与实际 GPU 渲染测试验证编码和消费者。
5. 不新增公共模板容器层级；只提供满足当前数组与角色访问需要的查询。目标签名、尺寸/格式变化和 fence retirement 原样保留。

## Risks / Trade-offs

- [映射漏项/重项] → 完整性与槽位/semantic 唯一性检查及旧 ABI 对照测试。
- [shader 仍有外部数值] → 保留 ABI 并用反射/渲染回归保护，不在本轮扩展 shader 生成体系。
- [错误放宽格式] → 保持原 format/设备能力拒绝行为并覆盖反例。

## Migration Plan

增加契约，迁移布局/消费者，运行契约、shader、Deferred、local/cluster/contact、深度约定和 Editor 渲染回归。无数据迁移；验收后再归档提交。

## Open Questions

无。
