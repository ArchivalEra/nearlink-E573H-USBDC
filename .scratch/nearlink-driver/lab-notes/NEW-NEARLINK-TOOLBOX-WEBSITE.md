# NearLink Toolbox website program knowledge

- Inspection date: 2026-09-11
- Source root: `/mnt/hdd/nearlink-stuff/nearlink-toolbox-website/`
- Mode: read-only web-source inspection; no build, no external download, no firmware execution
- Scope: application structure, actual runtime surface, download/update flow, claimed desktop features, and reuse boundaries for WS73/HHD-01 tooling

## Eight-line summary

1. This repository is a static Next.js marketing/documentation site, not the Tauri desktop toolbox described by its own `docs.md`.
2. The package is Next.js 15.1.11 with React 19 and a static `output: 'export'`; there is no Rust, Tauri, `src-tauri`, Cargo, or serial-port source tree.
3. The only executable download behavior is a browser button that fetches a remote `software-updater.json` and opens a Windows URL from that JSON in a new tab.
4. The site claims firmware store, firmware management, serial flashing, serial debugging, command libraries, and cloud mirrors, but the inspected source contains UI cards and screenshots rather than those backends.
5. `docs.md` describes React 18, TanStack Router, React Query, Tauri 2, Rust, `serialport`, Tokio, firmware signatures, rollback, and multi-device flashing; none of those implementations are present in this repository.
6. The update JSON is a runtime trust boundary: the TypeScript interface is not runtime validation, the URL is not allowlisted or checked, and no signature/checksum/size/platform verification is performed before opening it.
7. The page also injects a third-party 51.la analytics script with `screenRecord:true`, so the site has a privacy and supply-chain boundary beyond the download endpoint.
8. Reuse only the landing-page structure and user-flow vocabulary; a real WS73/HHD-01 toolbox needs a signed manifest, verified downloader, serial/USB flashing backend, device discovery, logs, rollback, and platform-specific packaging.

## 1. Actual application shape

The root page is a composition of presentation components: Header, Hero, Recognition, Showcase, Collaboration, Features, Integration, Documentation, ProjectGoals, TechStack, Performance, Statistics, Roadmap, Testimonials, Acknowledgments, and Footer. [app/page.tsx:0-16] The app has no API routes or server-side data layer in the inspected tree; `next.config.mjs` sets `output: 'export'` and unoptimized images, which is the build shape of a static site. [next.config.mjs:0-7]

The package metadata is generic: name `my-app`, version `0.1.0`, private, with scripts for `next dev`, `next build`, `next start`, and `next lint`. [package.json:0-9] Dependencies are React 19, Next.js 15.1.11, Radix UI, Tailwind, Recharts, Fancybox, and related UI libraries; there is no Tauri or Rust dependency. [package.json:10-50]

The repository contains no `Cargo.toml`, Rust source, `tauri.conf.json`, or serial-related source files. This is a structural finding from the source tree, not an assumption from the UI.

## 2. Documentation and implementation mismatch

`docs.md` describes a different product architecture:

- Tauri + React desktop application, temporarily Windows-only. [docs.md:4-5]
- React 18, TypeScript, TanStack Router, React Query, Radix UI, Tailwind, and CSS Modules. [docs.md:8-16]
- Rust backend on Tauri 2.0. [docs.md:17-22]
- Rust `serialport`, Tokio asynchronous I/O, automatic serial enumeration/reconnect, concurrent devices, text/hex transport, buffering, retries, and configurable serial parameters. [docs.md:26-53]
- Firmware version detection, incremental updates, rollback, integrity verification, signature verification, logging, permissions, and batch flashing. [docs.md:54-75]

The actual repository instead uses Next.js app-router components and static export. The mismatch is material: the documentation is an architectural description or roadmap, not a description of the checked-in implementation.

## 3. Download and update flow

`DownloadButton` is the only concrete download mechanism found. On click it opens a dialog and fetches:

```text
https://haohanyh-ctcc.gcxstudio.cn/software-updater.json
```

[components/DownloadButton.tsx:34-46] The response is assigned directly to `UpdateInfo`; the code checks `response.ok`, but does not validate schema, version, platform, URL origin, file size, checksum, signature, or release compatibility. [components/DownloadButton.tsx:6-16] [components/DownloadButton.tsx:34-52]

The final action is:

```ts
window.open(updateInfo.platforms['windows-x86_64'].url, '_blank')
```

[components/DownloadButton.tsx:113-116] There is no download progress, no file write, no installer launch, no signature verification, no fallback mirror selection, and no rollback path in this repository. The remote JSON therefore controls an externally opened URL and should be treated as a security-sensitive supply-chain input.

The README gives only generic Next.js development instructions and does not document the updater endpoint, manifest schema, release signing, or deployment process. [README.md:2-18]

## 4. Claimed features versus implemented behavior

The UI advertises:

- Firmware store, local firmware management, favorites, serial flashing, serial debugging, AT command favorites, and cloud data sources. [components/Features.tsx:7-56]
- One-click flashing, device recognition, automatic parameter configuration, batch flashing, logs, error diagnosis, and status monitoring. [components/Documentation.tsx:13-64]
- Integration of HiSilicon official flashing tools and `ws63flash`. [components/Integration.tsx:96-102]
- Official-tool stability, open-source extensibility, Windows support, planned Linux/macOS support, partition management, checksums, and error recovery. [components/Integration.tsx:44-94]
- Cloud firmware mirrors, serial terminal, flashing screenshots, and command collections. [components/Showcase.tsx:14-39]

The inspected source implements these as data-driven cards, tabs, dialogs, screenshots, and marketing text. It contains no serial enumeration, no serial read/write loop, no firmware manifest parser, no flashing command execution, no device-state machine, no checksum/signature verification, and no update installation logic. The `ws63flash` reference is a string in the UI, not an imported module or backend dependency.

## 5. Build and runtime boundaries

- Static export means the checked-in project is deployable as ordinary web assets; it is not a packaged desktop executable. [next.config.mjs:0-7]
- The page uses client-side animation observers and Fancybox effects, including `IntersectionObserver`, interval-driven text cycling, and confetti. [components/animations/AnimationWrapper.tsx:14-83] [components/Recognition.tsx:54-149]
- The page includes a third-party 51.la script directly in `app/layout.tsx` with `autoTrack:true`, `hashMode:true`, and `screenRecord:true`. [app/layout.tsx:63-70] This is an external script and telemetry boundary; it is unrelated to firmware download correctness but important for a production security review.
- The UI claims a 2024 award, 2024-2025 roadmap milestones, and usage statistics, but the statistics component uses a fixed six-row chart and a fixed `4.8/5.0` rating. [components/Statistics.tsx:10-17] [components/Statistics.tsx:212-226] [components/Roadmap.tsx:6-58] These are presentation data, not verified telemetry or release evidence.
- `ProjectGoals` claims `< 15秒` Hi3863 flashing at 921600 baud and approximately 99% availability without a benchmark or measurement source in this repository. [components/ProjectGoals.tsx:56-66]

## 6. Concrete code-level observations

- `DownloadButton` handles a failed JSON response by calling `response.json()` without guarding the content type; a non-JSON error body can throw inside the error branch and bypass the intended user-facing error. [components/DownloadButton.tsx:40-49]
- The updater interface permits only `windows-x86_64`; there is no explicit behavior for unsupported platforms, missing platform keys, malformed URLs, or non-HTTPS URLs. [components/DownloadButton.tsx:6-16]
- `window.open` is called without `noopener,noreferrer`; the opened page can retain opener access under browser behavior that permits it. [components/DownloadButton.tsx:113-116]
- The page's hard-coded external URL and analytics script make reproducibility depend on third-party availability and trust. [components/DownloadButton.tsx:34-46] [app/layout.tsx:63-70]
- The `TechStack` component labels Rust/Tauri/WebView/IPC as core technology even though no corresponding source is present. [components/TechStack.tsx:7-18]

## 7. Reuse for WS73/HHD-01 work

### Useful as a product/UX reference

- The section order is a reasonable landing-page pattern: overview, capabilities, flashing/integration, documentation, goals, architecture, performance, roadmap, and download.
- The feature vocabulary maps well to a future toolbox: firmware catalog, local firmware management, serial/USB flashing, terminal/debugging, AT-command library, cloud mirrors, logs, and device status.
- The dialog-based release-information flow can be retained only after adding manifest validation and a trusted download path.

### Not reusable as implementation

- Do not treat the site as evidence that Tauri, Rust, serialport, Tokio, firmware signing, rollback, or batch flashing exist.
- Do not reuse the remote updater URL or its unvalidated URL-opening behavior.
- Do not copy the fixed statistics, award claims, roadmap dates, or performance numbers as technical evidence.
- Do not use the site as a model for WS73 USB/DLI/SSAP transport or HHD-01 flashing protocol behavior.

### Minimum real-toolbox boundary

A production WS73/HHD-01 toolbox should define and enforce:

1. A versioned firmware manifest with device family, hardware revision, firmware version, platform, file size, checksum, signature, and release notes.
2. A trusted manifest transport and explicit allowlist/verification before any URL is opened or downloaded.
3. Atomic download, checksum/signature verification, resume/retry policy, and rollback or recovery path.
4. Device discovery and identity checks before flashing, with explicit WS73 USB versus HHD-01 serial/UART separation.
5. A serial/USB transport backend with bounded buffers, backpressure, timeouts, logs, and cancellation.
6. A flashing state machine that distinguishes bootloader, download, verify, reboot, and failure states.
7. Platform-specific packaging and signing, rather than a static web export that only opens an external Windows URL.

## Verified facts versus recommendations

### Verified from source

- The repository is a Next.js static website with React 19 and no Tauri/Rust/serial implementation.
- The only download behavior is fetching one remote JSON URL and opening a Windows URL from it.
- The UI makes broad firmware-tool claims without corresponding backend code.
- The documentation describes a materially different Tauri/Rust/React 18 architecture.
- The page injects a third-party analytics script with screen recording enabled.
- Statistics and roadmap data are hard-coded presentation values.

### Recommendations, not verified behavior

- Treat the site as a product-page artifact, not as a firmware-management implementation.
- Replace the remote updater with a signed, versioned, validated manifest and verified downloader.
- Build the actual flashing and serial/USB logic in a separate trusted backend.
- Keep WS73 USB and HHD-01 serial workflows as distinct device classes with explicit capability checks.
- Use the site only for UX vocabulary and information architecture until the implementation catches up with the documentation.
