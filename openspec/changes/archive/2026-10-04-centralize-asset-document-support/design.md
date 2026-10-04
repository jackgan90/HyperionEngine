## Context

AssetWorkspace::PollEntry 用四个 type ID 字符串，FAssetAutomation::Open 用四个 typeid 判断支持；两者随后创建同一个 FAssetEditDocument。底层 document 可用于反射草稿，本次不把其通用构造改为四类限制。

## Goals / Non-Goals

**Goals:** AssetEditing 拥有基础文档支持判断，适配器一致消费。
**Non-Goals:** 不新增资产类型、预览路由、field policy、operation/schema 或 lifecycle。

## Decisions

1. 提供接受 FRecordDescriptor 的纯查询，以规范 RecordType<T> 的 C++ 类型和类型 ID 确认 model/material/texture/sky；只有 AssetEditing 拥有此表。拒绝 scene 与未知或错配身份。
2. 在两个打开 admission 位置调用该查询，保留 Editor 原来的 failed/loading tabs、worker 初始化及独立 automation unsupported_type 和失败移除行为。
3. 不让基础支持推导预览支持：Editor Prepare/预览种类与已有能力限制保持独立；不限制通用 FAssetEditDocument 构造。
4. 通过契约测试验证四类与拒绝边界，复用 workspace、asset documents、automation assets/transport/discovery 等验证 shared lifecycle。

## Risks / Trade-offs

- [字段 policy 和基础支持概念混淆] → 支持查询是文档 admission，具体字段权限仍由 ResolveAssetFieldPolicy 决定。
- [头部与 descriptor 身份不一致] → asset loader 原有一致性校验仍保留；查询使用加载后的规范 descriptor。
- [预览错误吞并] → 仅替换 admission 白名单，保留预览、错误转换和页签行为。

## Migration Plan

增加纯查询与单元验证，替换两处列表并验证原有调用。无持久化或 wire 迁移；验收后再归档提交。

## Open Questions

无。
