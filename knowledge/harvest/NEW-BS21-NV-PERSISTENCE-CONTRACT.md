---
type: harvest
title: "BS21 NV persistence contract: completion callbacks, flush and region recovery"
language: en
created: 2026-09-17
tags: [harvest, bs21, nv, persistence, sdk]
sources:
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/include/middleware/utils/nv.h"
trust: B
stale_after: 2027-03-17
---

# BS21 NV persistence contract: completion callbacks, flush and region recovery

## Executive findings

This report records the public declaration and documentation contract in `include/middleware/utils/nv.h` at revision `f4f3f28781f4610a1d0a97fe24907d59ed03d09d`. It does not claim that the storage implementation, power-loss behavior, or cryptography has been verified. All line references below are to this pinned header.

1. **A write return and a flash-completion callback are distinct interface events.** `uapi_nv_write` accepts a 16-bit key and 16-bit byte length and uses normal attributes without a callback (`:174-197`). `uapi_nv_write_with_attr` additionally takes attributes and a callback documented as running after the value is written to flash (`:200-231`). The callback typedef receives only an `errcode_t`, with no key, request identifier, or user context (`:30-37`). An asynchronous wrapper must not assume it can identify concurrent requests from the callback argument alone. [Write declarations](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/include/middleware/utils/nv.h#L174-L231).
2. **Flush has a configuration-dependent contract.** `uapi_nv_flush` is documented as ensuring RAM data reaches flash, but only being effective when asynchronous storage support is enabled (`:375-392`). The declaration alone does not specify timeouts, task context, ordering across callers, or whether ordinary writes are synchronous in other configurations. [Flush declaration](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/include/middleware/utils/nv.h#L375-L392).
3. **Attributes contain irreversible-looking policy constraints, not just flags to toggle later.** `nv_key_attr_t` contains `permanent`, `encrypted`, `non_upgrade`, and a reserved byte (`:55-64`). The write-with-attributes documentation says encryption and permanent attributes cannot be modified, and a permanent key's value cannot be modified (`:204-205`). The `non_upgrade` English comment is ambiguous; its name must not be promoted into a fully verified upgrade policy. No cipher, key management, or authentication guarantee is established by the `encrypted` field. [Attributes](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/include/middleware/utils/nv.h#L55-L64).
4. **Reads separate caller capacity from returned length.** `uapi_nv_read` takes `kvalue_max_length`, an output length pointer, and an output buffer; `uapi_nv_read_with_attr` also returns attributes (`:233-292`). The interface does not explain whether an undersized buffer truncates or fails, nor the output validity after an error. A consumer must check the return status before interpreting the returned length or data. [Read declarations](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/include/middleware/utils/nv.h#L233-L292).
5. **Recovery selection is by key-ID region, not by an arbitrary per-key list.** Sixteen enumerated regions cover keys `0x0001` through `0xFFFF`; region zero excludes zero and the final region includes `0xFFFF` (`:96-131`). Backup and restore mode structs contain one boolean per region (`:133-155`). Backup has its own API (`:315-334`), while the restore APIs explicitly set all/partial restore flags (`:336-372`). Do not equate setting a restore flag with an immediate completed restoration. [Region and mode definitions](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/include/middleware/utils/nv.h#L96-L155).
6. **Capacity status distinguishes space classes.** `nv_store_status_t` reports total, used, reclaimable, corrupted, and maximum-single-key space (`:73-87`); `uapi_nv_get_store_status` exposes the query (`:294-313`). Comments say reclaimable and corrupted space can be reused after erasing, not that it is immediately free. The header does not provide an accounting identity or justify subtracting all fields from total. [Status definition](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/include/middleware/utils/nv.h#L73-L87).

## Lifecycle and notification boundary

Initialization is required before using NV functions, but the actual `uapi_nv_init` declaration returns `void` (`:157-172`) despite return-value documentation above it. Bindings must follow the declaration rather than manufacture an error return from the prose.

Change notifications are separate from storage-completion callbacks: `nv_changed_notify_func` receives a key (`:39-46`), and `uapi_nv_register_change_notify_proc` registers a minimum/maximum key range (`:394-415`). The header does not establish callback scheduling, range endpoint inclusivity, reentrancy, or a corresponding unregister operation. A change notification is not documented here as a durable-write acknowledgment.

## Reusable guidance

- For a future persistence abstraction, distinguish submission status, durable completion, and value-change notification. Preserve their different callback payloads instead of combining them into one event.
- Keep key-region allocation in the application's data model when designing selective factory restoration. Verify actual restore timing in an implementation or controlled test before relying on it.
- Make permanent and encrypted attribute choices explicit at initial key creation. Do not assume a later ordinary write changes those attributes.
- Query maximum-single-key capacity rather than treating the API's 16-bit length type as a guaranteed supported payload size.
- For a Rust binding, use target-appropriate C representations; this header does not establish a portable serialized struct layout or a cross-chip storage format.

These are design recommendations, not changes to our WS73 stack. No device key was read or written, no restore/backup/flush call was executed, no code was imported, and no build or runtime test was performed.

## Comparison and deduplication

[NEW-BS21-WTSL](NEW-BS21-WTSL.md) provides the Ai-BS21 SDK lineage and radio API overview. This report adds the public NV persistence contract rather than repeating its SLE/BLE sample inventory. Searches of the existing knowledge bundle found no prior coverage of `nv_key_attr_t`, `nv_storage_completed_callback`, `uapi_nv_flush`, or `uapi_nv_set_restore_mode_partitial` before this report.

The inspected SDK tree exposes storage-layer headers, but this report deliberately limits evidence to the public header. Atomicity, buffer lifetime until completion, wear leveling, encryption strength, and compatibility with WS73 persistence remain unverified; they cannot be inferred from these declarations.
