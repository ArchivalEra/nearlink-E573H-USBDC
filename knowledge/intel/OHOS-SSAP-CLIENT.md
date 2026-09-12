---
type: intel
title: OpenHarmony SSAP 客户端解剖（ssapc_*）
language: zh
created: 2026-08-17
tags: []
---

# OpenHarmony SSAP 客户端解剖（ssapc_*）

Date: 2026-08-17
任务: 以 OHOS 权威 ssapc 实现为蓝本，补齐我方只有编码器的 SSAP 客户端。

## Sources
- 主源 `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/cp/bsl/sle/servm/ssap/`（下文行号均相对此目录）：
  `src/ssapc_client.c`(1682), `src/ssapc_cache.c/h`, `src/ssapc_app.c`, `src/ssapc_app_link_sm.c`, `src/ssapc_client_api.c/h`, `src/nlstk_ssap_app_client.c`, `src/ssap_manager.c`, `include/inner/ssap_pkt.h`, `include/inner/ssapc_app.h`, `include/nlstk_ssap_app_client.h`, `include/nlstk_ssap_app_link.h`
- 我方对照: `stack/ssap/src/ssap_codec.c`（仅编码器）; 服务端解剖见 `OHOS-SSAP-ENGINE.md`

## 1. 客户端核心流程 / 状态机
- 状态机: `ssapc_app_link_sm.c:48-92` — 2D 表 `[SSAP_CONNECT_STATE_{IDLE,CONNECTING,CONNECTED,DISCONNECTING,DISCONNECTED}][SSAP_USER_CONNECT/USER_DISCONNECT/LOGIC_LINK_CONNECTED/LOGIC_LINK_DISCONNECTED]`（事件见 `include/inner/ssapc_app.h:28-34`）。每 appId 一个状态；当 CM 忙时把下次操作缓存于 `nextOperator`（`ssapc_app.c:100-118`），链路事件到达后重放。
- 建链上报 CM_CONNECTED 时（`ssap_manager.c:283-298`）: 创建 link + `SsapcCacheCreate(addr)` + 排队首个 `SSAPC_InitFindReq`（FIND_STRUCTURE_REQ, type=PRIMARY_SERVICE, [0x0001,0xFFFF]，"确保连接管理能力查询完成"，rsp 丢弃；适配老协议栈）。断链时 `SsapcCacheDestroy`。
- 数据组织: 全局 `g_ssapcCache`（vector of `SsapcCache_S`，按 addr 索引，`ssapc_cache.c:28-37`）。每地址含 `serv`(发现中)/`finishedServ`(完成)/`cpcds` 三 vector + `servFindFinish`/`isByUuid`/`curFindUuid`（`ssapc_cache.h:29-37`）。服务节点 `SsapCacheServ_S{structure{handle,endHandle,uuid,serviceType,memberValue}, prop/method/eventFindFinish, properties/methods/events vector}`（`ssapc_cache.h:47-55`）。memberValue 位图 + 各 FindFinish 标志驱动发现推进。
- 发现推进（`ssapc_app.c`）: `SsapcAppDiscServ`(439) → 若 cfgdb/version<1.3 把 SERVICE_STRUCTURE 降级为 PRIMARY_SERVICE(452-457) → 完成回调 `SsapcAppDiscServCompCb`(420) 按 opCode 分发到 `SsapcAppDiscServByHandleCompCb`(329)。SERVICE_STRUCTURE 失败→回退 PRIMARY_SERVICE(330-335)。PRIMARY_SERVICE 反复发 `[maxHandle+1,0xFFFF]` 直到 ITEM_INEXIST 或 max==0xFFFF(348-368)，随后 `SsapcFindNextMember`(282-300) 用 `SsapcCacheGetNextFindMember` 逐服务取下一未完成成员类型（cache.c:432-457，start=maxMemberHandle+1，按 property→method→event 顺序），ITEM_INEXIST 时 `SsapcCacheServMemberDiscFinish`(459)，全完则 `SsapcCacheServDiscFinish` + 上层 `onFindService`(ssapc_app.c:246-272)。

## 2. FIND 响应解析（ssapc_client.c:748-794 分发）
- 头 `SSAP_PduFindStructRsp_S{msgCode, ctrl{fragment:2,itemType:2}}`（ssap_pkt.h:269-277）。按 req 的 `ctrl.findType` + `msgCode`(FIND vs FIND_BY_UUID) + `CM_GetLogicLinkDeviceType==CM_DEVTYPE_OLD` 选布局（777-792）。
- v1.3 布局: 主服务 item `{start u16, end u16, uuid(2/16), memberValue u8}`（188-206）；属性/方法/事件 item `{handle u16, uuid, operationValue u32, descriptorCount u8, descriptors[count]}`（333-361）。itemType=STANDARD→uuid16，CUSTOMIZE→uuid128，MIX→按 `SSAP_FindInfoIndicator{count:7,type:1}` 分组（ssap_pkt.h:279-282; 解码 253-296）。
- v1.0 布局: item 内无 uuid/memberValue 前置，uuid 取请求 UUID（`SSAP_Decode*V10` 208-295），仅 OLD 设备走此路径。
- SERVICE_STRUCTURE 模式（`SSAP_DecodeMixStructure` 627-674）: 混合成员流，每 item `{handle u16, structureType u8, uuid, operation u32, descriptorCount u8, descs}`；用 `SsapcCacheSortCachedServ`(cache.c:553-601) 在遇到新服务声明时对上一服务按 handle 排序并推导 endHandle/memberValue；长度不整时"尽力解析"丢弃尾部。
- MTU 边界: rsp 长度 <= `SSAP_STACK_MTU_MAX`(1024) 才解析（ssapc_client.c:752）。

## 3. 读 / 写 / 通知订阅
- READ_REQ: `{msgCode, ctrl{fragment:2}, items{handle u16,type u8}×N}`，单值 `SSAPC_ReadReq`(1450) / 多值 `SSAPC_ReadProps`(1477)。READ_RSP ctrl`{fragment:2,multi:1,error:1}`（ssap_pkt.h:317-335）。单值成功=items 即原始 value（`SSAP_READ_RSP_DATA_OFFSET=2`），失败=1 个 `SSAP_PduReadRspItem{length:15=errcode, success:1}`（796-834）。多值=循环 `{length:15,success:1,value}`，item 数须等于请求 handle 数（836-908）。
- READ_BY_UUID_REQ: `{uuidType ctrl, startHandle, endHandle, dataType, uuid}`（ssap_pkt.h:347-357; 编码 1506-1537）。RSP items: `{handle u16, ...}`；ctrl 位 4=multi、位 8=err（`SSAP_READ_BY_UUID_RSP_MULTI/ERR_CONTROL`）；每实例 `{handle, indication u16}`，indication bit15=错误标志、低 15 位=len 或错误码（946-993）。
- WRITE_CMD: `{fragment,multi:1,oper:2}` + `{handle,type,value}`，无 rsp（1542-1569；API `SSAP_WriteCmd` 无 timeout/cb，ssapc_client_api.h:56）。WRITE_REQ: ctrl 加 `verify:1`，客户端默认置 1=原值回显（1574-1601；ssap_pkt.h:429-448）。WRITE_RSP: ctrl`{result:2}` 0=成功/1=部分/2=取消；成功+verify→回显 `{handle,type,value}`，纯成功 2 字节；错误→`{errorNum, errList[{handle,errCode}]}`（1078-1119；ssap_pkt.h:470-497）。
- CCCD 订阅: 描述符 handle 上读写 `DESC_TYPE_CLIENT_CONFIG`，值 u16 LE：0=disable / 1=NOTIFICATION_ENABLE / 2=INDICATION_ENABLE（`ssapc_cache.h:24-27`）。Set=`SsapcAppSetCpcd` 先写本地 `SsapcCacheSetCpcd`（按 handle+appId 的 config vector，ssapc_cache.c:832-878）再 WRITE_REQ；重复同 handle 第二个 app 直接回调（`NLSTK_ERRCODE_DIRECT_RETURN`，ssapc_app.c:885-928）。Get=READ_REQ 同类型对比回读值（752-812）。
- VALUE_NTF（`SSAPC_ValueNtfHandle` 1173-1202）: 循环 `{handle u16,length u16,value}`；特判 `SSAP_SERVICE_CHANGE_EVENT_HANDLE` 触发服务变更重发现（1121-1131）。`SsapcAppPropertyNtf`(1227) 按 ctrl 位 2 分流 property/event；property 通知只投递到 cpcd 匹配的 appId（1179-1207），event 广播该 addr 全部 app（1209-1225）。
- VALUE_IND（1278-1290）: 解析同 NTF，随后**必须自动回发** `SSAP_VALUE_ACK`，每 item 一个字节 `SSAP_ACK_RECV_SUCCESS`（`SSAPC_ValueIndSendAck` 1248-1272；编码 `SSAPC_ValueAck` 1606-1629）。

## 4. 应用层 API 调用序列（nlstk_ssap_app_client.c）
`NLSTK_SsapClientRegApp`(56) → `NLSTK_SsapClientDiscoverServices`(182)（参数固定 [0x0001,0xFFFF]）→ 回调 `onFindService(appId,err)` → `NLSTK_SsapClientGetServices[Asyn/ByUuid]`(235-324) 从缓存深拷贝，返回 free func 由调用方释放（ssapc_app.c:516-549）→ 读写订阅。全部 API 经 `SchedulePostTask*` 投递到 `SsapcApp*` 处理函数，参数 malloc 拷贝、回调后释放。连接状态/超时/MTU 交换见 `NLSTK_SsapClientRegAppAsyn(93)/SetInteractionTimeout(151)/ExchangeMtu(168)`。回调集合 `NLSTK_SsapAppClientCb_S`（nlstk_ssap_app_client.h:194-216）: onConnectionStateChanged/onFindService/onGetServices/onMtuChanged/onReadProperty(s|ByUuid|Descriptor)/onSet|GetPropertyNtf|Ind/onPropertyChanged/onWriteProperty|Descriptor/onCallMethod/onEvent/onServiceChange|Rediscover。

## 5. 我方实现蓝本（stack/ssap/ 新增 ssap_client 模块）
建议文件: `ssapc_client.c`(PDU 收发+解析)、`ssapc_cache.c`(每 addr 发现缓存)、`ssapc_app.c`(应用编排+回调分发)、`ssapc_client_api.c`(带 timeout 的 task 封装)、`ssapc_app_link_sm.c`(建链 FSM)。编码器复用现有 `ssap_codec.c`。需实现的函数（签名级）:
- 发现: `int ssapc_discover_services(int appId, uint8_t findType)`; `int ssapc_find_req(addr,type,itemType,start,end)`; `int ssapc_find_by_uuid_req(...)`; `void ssapc_find_rsp_handle(link, buff)`(v1.0/v1.3/mix/struct 分发)
- 缓存: `ssapc_cache_create/destroy(addr)`; `ssapc_cache_serv/prty/method/event(addr,item)`; `ssapc_cache_get_next_find_member(addr,&type,&start,&end)`; `ssapc_cache_serv_member_disc_finish(addr,type,handle)`; `ssapc_cache_serv_disc_finish(addr)`; `ssapc_cache_get_services(addr,appId,uuid,&serv,&num)`
- 读: `ssapc_read_req(handle,type)` / `ssapc_read_props(handles[],num,type)`; `ssapc_read_by_uuid_req(uuid,start,end,type)`; `ssapc_read_rsp_handle(...)`(单/多值+错误 item); `ssapc_read_by_uuid_rsp_handle(...)`
- 写: `ssapc_write_cmd(handle,type,value)`; `ssapc_write_req(handle,type,value,verify)`; `ssapc_write_rsp_handle(...)`(result/errorList)
- 通知: `ssapc_value_ntf_handle(...)`; `ssapc_value_ind_handle(...)`; `ssapc_value_ack(link,count)`; `ssapc_set_cpcd(appId,handle,enable,isNtf)` / `ssapc_get_cpcd(...)`
- 建链: `ssapc_link_state_machine(appId,event,reason)`; `ssapc_init_find_req(link)`（连接后首个报文）

## Open questions
- 分片: 各 RSP 仅校验 `<=MTU_MAX`，fragment!=NO_FRAG 的重组在哪层完成？OHOS 在 link 收包层，我方需自研（见 ssap_link.c）。
- `CM_DEVTYPE_OLD` 判定与 v1.0 布局耦合，我方对 WS73 需先握手确定版本再选解析路径。
- `SSAP_EXCHANGE_INFO_REQ` 在 OHOS 非自动触发（用户 `ExchangeMtu` 调用，ssapc_client.c:1347-1370），但 `link->mtu/version/fragment/multiProcessing` 初始值依赖它；我方建议连接后自动交换。
- 多值读（ReadProps）由 `link->multiProcessing || cfgdb` 门控（ssapc_app.c:1297-1305），老设备需回退单值循环。
