---
type: harvest
title: "NEW-NEARLINKERNEL-CLASSIFICATION"
language: en
created: 2026-09-12
tags: [harvest, nearlinkernel, classification]
sources:
  - "https://github.com/f-t-s/nearLinKernel"
trust: B
stale_after: 2027-03-12
---

# NEW-NEARLINKERNEL-CLASSIFICATION

## Scope and inspection boundary

This report classifies the locally inspected repository `https://github.com/f-t-s/nearLinKernel` for possible relevance to HiSilicon NearLink/SparkLink work.
The inspection was read-only and local; no network access, build, test execution, or hardware interaction was performed.
The repository README describes the tree as code used for numerical examples in a paper titled “Compression, inversion, and approximate PCA of dense kernel matrices at near-linear computational complexity.” [`https://github.com/f-t-s/nearLinKernel`]
The README links the paper rather than describing a wireless product, driver, SDK, or protocol implementation. [`https://github.com/f-t-s/nearLinKernel`]

## Executive classification

**Classification: name collision, not a HiSilicon NearLink/SparkLink project.**
The repository name appears to combine “near-linear” with “kernel,” matching the paper title's “near-linear computational complexity” and “dense kernel matrices,” rather than the NearLink radio technology. [`https://github.com/f-t-s/nearLinKernel`]
The inspected code contains numerical-linear-algebra modules and experiment scripts, not a Linux kernel driver or communications stack. [`https://github.com/f-t-s/nearLinKernel`]
No inspected file mentions HiSilicon, WS63, WS73, SparkLink, SLE, SSAP, HCI, USB transport, radio firmware, or a dongle. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The only “kernel” concept present is a mathematical covariance/kernel-matrix concept used by the numerical experiments. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The only “near-linear” concept present is the complexity claim in the paper title, not a NearLink protocol or device family. [`https://github.com/f-t-s/nearLinKernel`]
There is no evidence of a Linux kernel module, kernel-space API, device probe, URB handling, character device, ioctl surface, or hardware register access. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
There is no evidence of a wireless protocol implementation, packet parser, state machine, link manager, security layer, or RF configuration. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
There is no evidence of vendor SDK integration, firmware packaging, board bring-up, serial flashing, or device-specific build instructions. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The appropriate harvesting decision is to exclude this repository from future NearLink harvesting as a false positive caused by the similar name. [`https://github.com/f-t-s/nearLinKernel`; `.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:6-8`]

## Complete tracked-file inventory

The tracked non-Git inventory consists of one Markdown README and nineteen Julia source files; each entry below is represented in the inspected tree. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
1. `https://github.com/f-t-s/nearLinKernel/blob/master/README.md` — project description and paper link. [`https://github.com/f-t-s/nearLinKernel`]
2. `https://github.com/f-t-s/nearLinKernel/blob/master/src/Cauchy_Vary_rho.jl` — Cauchy-kernel scaling experiment. [`https://github.com/f-t-s/nearLinKernel`]
3. `https://github.com/f-t-s/nearLinKernel/blob/master/src/GenerateHighDimPoints.jl` — synthetic ellipse, spiral, and mixed-point generators. [`https://github.com/f-t-s/nearLinKernel`]
4. `https://github.com/f-t-s/nearLinKernel/blob/master/src/Matern_highDim_vary_rho.jl` — high-dimensional Matérn experiment. [`https://github.com/f-t-s/nearLinKernel`]
5. `https://github.com/f-t-s/nearLinKernel/blob/master/src/Matern_Vary_dz.jl` — Matérn experiment varying a z-axis deformation parameter. [`https://github.com/f-t-s/nearLinKernel`]
6. `https://github.com/f-t-s/nearLinKernel/blob/master/src/Matern_Vary_N.jl` — Matérn experiments varying problem size in two and three dimensions. [`https://github.com/f-t-s/nearLinKernel`]
7. `https://github.com/f-t-s/nearLinKernel/blob/master/src/Matern_Vary_nu.jl` — Matérn experiment varying smoothness `nu`. [`https://github.com/f-t-s/nearLinKernel`]
8. `https://github.com/f-t-s/nearLinKernel/blob/master/src/Matern_Vary_rho.jl` — Matérn experiment varying the sparsity/oversampling parameter `rho`. [`https://github.com/f-t-s/nearLinKernel`]
9. `https://github.com/f-t-s/nearLinKernel/blob/master/src/PlotBoundaryEffects.jl` — boundary-conditioning and Schur-complement visualization. [`https://github.com/f-t-s/nearLinKernel`]
10. `https://github.com/f-t-s/nearLinKernel/blob/master/src/plotMaternKernel.jl` — Matérn function and spectrum plots. [`https://github.com/f-t-s/nearLinKernel`]
11. `https://github.com/f-t-s/nearLinKernel/blob/master/src/PlotOrderings.jl` — ordering and sparse-factor visualization. [`https://github.com/f-t-s/nearLinKernel`]
12. `https://github.com/f-t-s/nearLinKernel/blob/master/src/PlotPCA.jl` — PCA-versus-Cholesky approximation plot. [`https://github.com/f-t-s/nearLinKernel`]
13. `https://github.com/f-t-s/nearLinKernel/blob/master/src/PlotSigmaEffect.jl` — nugget/sigma effect on covariance and inverse factors. [`https://github.com/f-t-s/nearLinKernel`]
14. `https://github.com/f-t-s/nearLinKernel/blob/master/src/Utils.jl` — Laplace-grid and boundary-point utilities. [`https://github.com/f-t-s/nearLinKernel`]
15. `https://github.com/f-t-s/nearLinKernel/blob/master/src/spelliptic/CovFuncs.jl` — covariance-function definitions. [`https://github.com/f-t-s/nearLinKernel`]
16. `https://github.com/f-t-s/nearLinKernel/blob/master/src/spelliptic/Diagnostics.jl` — approximation-error, scatter, and rank diagnostics. [`https://github.com/f-t-s/nearLinKernel`]
17. `https://github.com/f-t-s/nearLinKernel/blob/master/src/spelliptic/Factorization.jl` — incomplete Cholesky implementation. [`https://github.com/f-t-s/nearLinKernel`]
18. `https://github.com/f-t-s/nearLinKernel/blob/master/src/spelliptic/MutHeap.jl` — mutable maximum heap implementation. [`https://github.com/f-t-s/nearLinKernel`]
19. `https://github.com/f-t-s/nearLinKernel/blob/master/src/spelliptic/SortSparse.jl` — sparse ordering and “daycare” data structure. [`https://github.com/f-t-s/nearLinKernel`]
20. `https://github.com/f-t-s/nearLinKernel/blob/master/src/spelliptic/Spelliptic.jl` — main sparse-operator module and public API. [`https://github.com/f-t-s/nearLinKernel`]

## Language, build, and runtime inventory

The implementation language is Julia, as shown by the `.jl` files, `module` declaration, `function` definitions, and Julia package imports. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The repository is not a C, C++, Rust, Python, or Go communications implementation; the tracked source set is Julia plus the Markdown README. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The main module imports `IterativeSolvers`, `SparseArrays`, `Distributions`, `Random`, `LinearAlgebra`, and `Base.Threads`, which identifies a numerical-computing runtime rather than a driver runtime. [`https://github.com/f-t-s/nearLinKernel`]
The covariance module imports `SpecialFunctions` and defines Matérn, Gaussian, exponential, inverse-multiquadratic, rational-quadratic, and Cauchy functions. [`https://github.com/f-t-s/nearLinKernel`]
The experiment scripts include the main module, covariance functions, and diagnostics, showing that they are executable research scripts rather than installable protocol components. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The scripts use plotting and typesetting packages including `Plots`, `PyPlot`, `TexTables`, and `LaTeXStrings`. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The high-dimensional data generator uses `Rotations` and `Distributions` to create synthetic point clouds. [`https://github.com/f-t-s/nearLinKernel`]
The sparse-ordering implementation additionally uses `StaticArrays` and `LinearAlgebra`. [`https://github.com/f-t-s/nearLinKernel`]
The code contains no conventional build manifest, Makefile, CMakeLists file, package project file, or shell entrypoint in the inspected tracked set; its entrypoints are top-level Julia scripts and included modules. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The scripts write LaTeX tables under `./out/`, for example `vary_rho_cauchy...tex`, `vary_rho_matern...tex`, and `vary_N_matern...tex`. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The plotting scripts write PDFs, PNGs, and JPEGs under `../figures/`, showing a paper-figure generation workflow. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The code uses multithreading imports, but the inspected files do not define a device-service process, daemon, kernel thread, or hardware interrupt path. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The experiments are large-scale numerical workloads: several scripts set `N = 1000000`, use 50 repetitions, and draw 500000 diagnostic samples. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The runtime therefore appears to be an offline Julia research environment with plotting backends, not a real-time or embedded runtime. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]

## Protocol and domain inventory

The domain is numerical linear algebra for dense kernel matrices, specifically compression, inversion, and approximate PCA. [`https://github.com/f-t-s/nearLinKernel`]
The central abstraction is `AbstractSpelMat`, described as an operator represented by a sparse Cholesky factorization. [`https://github.com/f-t-s/nearLinKernel`]
`SpelMat` stores the number of degrees of freedom, nonzero count, elimination permutation, reverse permutation, and a sparse upper-triangular factor. [`https://github.com/f-t-s/nearLinKernel`]
Its constructor accepts a `d × N` array of points and an accuracy factor `rho`, then calls `sortSparse` to construct the sparsity pattern. [`https://github.com/f-t-s/nearLinKernel`]
The constructor builds a Julia `SparseMatrixCSC`, transposes it, and returns the sparse operator. [`https://github.com/f-t-s/nearLinKernel`]
Matrix conversion reconstructs a dense matrix as `U' * U` and applies the reverse elimination ordering. [`https://github.com/f-t-s/nearLinKernel`]
The module overloads logarithmic determinant, multiplication, left division, right division, indexing, and related linear-algebra operations. [`https://github.com/f-t-s/nearLinKernel`]
`setCovStat!` fills sparse-factor entries from pairwise point distances, maps a supplied covariance function over those distances, applies a scale `eta`, and optionally adds a nugget `sigma`. [`https://github.com/f-t-s/nearLinKernel`]
The code explicitly warns that larger nugget terms degrade the approximation property and recommends `SpelMatIter` for large nuggets. [`https://github.com/f-t-s/nearLinKernel`]
`SpelMatIter` stores both the compressed factor and a preconditioning factor, plus `sigma` and tolerance fields. [`https://github.com/f-t-s/nearLinKernel`]
The iterative constructor creates a second sparse CSC preconditioner from the same sparsity pattern. [`https://github.com/f-t-s/nearLinKernel`]
Its covariance setup copies values into both the main factor and preconditioner and runs incomplete Cholesky on both. [`https://github.com/f-t-s/nearLinKernel`]
The iterative operator adds the nugget term directly to matrix-vector products. [`https://github.com/f-t-s/nearLinKernel`]
A wrapper applies the preconditioner before and after the compressed operator, while conjugate gradients solve the preconditioned system. [`https://github.com/f-t-s/nearLinKernel`]
The diagnostics module compares exact covariance values with approximated entries and returns true, approximate, error, and distance vectors. [`https://github.com/f-t-s/nearLinKernel`]
A second diagnostic path excludes points near a boundary, which is useful for interior-error experiments. [`https://github.com/f-t-s/nearLinKernel`]
The diagnostics also estimate Frobenius and relative Frobenius errors over repeated random samples. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The factorization module implements an in-place incomplete Cholesky routine over sparse-column pointers and row indices. [`https://github.com/f-t-s/nearLinKernel`]
The ordering module implements a mutable maximum heap with a lookup array for node positions. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The same module implements heap update and move-down operations used to maintain nearest-parent distances. [`https://github.com/f-t-s/nearLinKernel`]
`SortSparse.jl` defines a “daycare” structure that tracks descendants of each elimination parent in buffered sparse columns. [`https://github.com/f-t-s/nearLinKernel`]
Its child-selection logic tests a distance threshold based on `lengthscale * rho`, updates the heap, and may update a preferred parent. [`https://github.com/f-t-s/nearLinKernel`]
The Euclidean wrapper computes squared distances by summing coordinate differences, then returns column pointers, row values, and permutations. [`https://github.com/f-t-s/nearLinKernel`]
The experiment scripts vary `rho`, `N`, `nu`, and a deformation parameter `dz`, confirming that the repository studies algorithmic scaling and approximation quality. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The scripts repeatedly construct `SpelMat`, fill covariance values, time sorting and factorization, measure rank and sparsity, and estimate approximation error. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The boundary-effects script forms dense covariance matrices and a Schur complement after conditioning on boundary points. [`https://github.com/f-t-s/nearLinKernel`]
The PCA script compares truncated Cholesky and eigenvalue approximations against a dense Matérn covariance matrix. [`https://github.com/f-t-s/nearLinKernel`]
The sigma-effect script studies `ExpKOrd + sigma * I` and the corresponding inverse, again in a numerical-analysis setting. [`https://github.com/f-t-s/nearLinKernel`]
No file defines packet framing, service discovery, connection management, SSAP properties, DLI commands, HADM ranging, PHY parameters, or USB endpoint behavior. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]

## Reusable concepts and transfer limits

The sparse Cholesky representation is a reusable numerical concept for low-rank or sparse approximation, but it is not a communications abstraction. [`https://github.com/f-t-s/nearLinKernel`]
The elimination-ordering and sparsity-pattern construction could inspire generic preprocessing or nearest-neighbor indexing, but the implementation is tightly coupled to Julia sparse matrices and Euclidean point data. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The covariance-function interface is a clean example of passing a mathematical kernel as a callable object and applying it to sparse distances. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The nugget/preconditioner split is a reusable numerical idea for stabilizing ill-conditioned systems, not a radio-link flow-control mechanism. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The conjugate-gradient wrapper demonstrates a preconditioned iterative solve around a compressed operator. [`https://github.com/f-t-s/nearLinKernel`]
The mutable heap with a reverse lookup table is a reusable data-structure pattern for updating priorities by identifier. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The “daycare” structure is an application-specific buffered sparse-column representation for elimination descendants. [`https://github.com/f-t-s/nearLinKernel`]
The diagnostic functions provide a reusable pattern for sampling exact versus approximate entries and reporting absolute or relative Frobenius error. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The synthetic generators provide reusable patterns for controlled low-dimensional manifolds embedded in higher-dimensional spaces. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The plotting scripts provide reusable paper-figure patterns for covariance curves, spectra, ordering heat maps, and approximation-error comparisons. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
Direct code reuse in the NearLink driver effort would be poor because the source is Julia research code while the target effort is a Linux host/dongle protocol and driver project. [`https://github.com/f-t-s/nearLinKernel`; `.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`]
The numerical concepts do not supply wire-format knowledge, device enumeration, USB transfers, firmware state machines, or SSAP semantics needed by the WS73 work. [`https://github.com/f-t-s/nearLinKernel`; `.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`]
At most, the repository could be cited as an example of sparse-matrix experimentation if a future NearLink task independently needs numerical modeling; it should not be harvested as a NearLink implementation source. [`https://github.com/f-t-s/nearLinKernel`; `.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:197-204`]

## Comparison with RESEARCH-DIRECTIONS.md

`RESEARCH-DIRECTIONS.md` is explicitly centered on WS63/WS73 HiSilicon work, including WiFi/SLE coexistence, dual-dongle connection, SSAP, DLI, HADM ranging, and the WS73 USB driver skeleton. [`.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:6-16`; `.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:31-44`]
That document treats the WS73 path as a five-endpoint HCC USB design with a proposed `/dev/ws73hci` character device and firmware-download state machine. [`.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`]
It also records protocol-level work on SSAP, DLI opcodes, ACB credit control, HADM commands, and USB transport contracts. [`.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:51-64`; `.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:194-211`]
By contrast, `nearLinKernel` contains no device-specific ABI, no endpoint management, no command/event opcodes, and no transport framing. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
The research directions value OpenSparklink and WS63 sources for byte-level protocol cross-checking, whereas `nearLinKernel` provides only mathematical covariance and factorization code. [`.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:51-64`; `https://github.com/f-t-s/nearLinKernel`]
The only superficial similarity is the word “kernel”: the NearLink documents use kernel in the Linux-driver sense or not at all, while this repository uses kernel in the statistical/covariance-matrix sense. [`.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`; `https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
No concept from `nearLinKernel` maps directly to the NearLink harvesting priorities of SSAP behavior, DLI transport, USB contracts, HADM ranging, or vendor firmware bring-up. [`.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:31-64`; `https://github.com/f-t-s/nearLinKernel`]

## Comparison with COMMUNITY-PROJECTS.md

`COMMUNITY-PROJECTS.md` defines useful NearLink projects as chip-side or host-side SLE implementations, protocol documentation, firmware tooling, or vendor SDK references. [`.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:6-8`; `.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:16-32`]
Its high-value examples include NearLinkSLE for SSAP call order, `sle_measure_sdk` for PHY/MCS/CI and credit-gated sending, and the BS21E SDK for the SLE-Link wire specification. [`.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:70-88`; `.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:122-129`]
That report explicitly distinguishes transferable protocol/API knowledge from code that cannot be reused because it runs inside vendor chip SDKs. [`.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:197-204`]
`nearLinKernel` fits neither side of that distinction: it is not a chip-side SLE sample, not a host-side SLE stack, not firmware tooling, and not a vendor SDK reference. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
Its Julia covariance code has less NearLink relevance than even the community report's “edge” projects, because the edge projects at least demonstrate a serial bridge or product topology involving SLE hardware. [`.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:37-49`; `https://github.com/f-t-s/nearLinKernel`]
The community report's reusable concepts are SSAP sequencing, CCCD notification behavior, ACB flow control, PHY tuning, frame aggregation, and SLE-Link TLV/CRC framing; none is implemented in `nearLinKernel`. [`.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:143-160`; `https://github.com/f-t-s/nearLinKernel`]
The community report also identifies the WS73 vendor SDK as a confirmation baseline rather than a source of new protocol knowledge; `nearLinKernel` is even further removed because it has no HiSilicon SDK tree or device code. [`.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:131-134`; `https://github.com/f-t-s/nearLinKernel`]

## Harvesting decision

**Exclude `nearLinKernel` from future NearLink harvesting.**
Reason 1: the repository's own README identifies a numerical-examples paper, not a NearLink, SparkLink, HiSilicon, or Linux-driver project. [`https://github.com/f-t-s/nearLinKernel`]
Reason 2: the complete source inventory is Julia numerical analysis code, with no wireless, USB, kernel-driver, firmware, or protocol files. [`https://github.com/f-t-s/nearLinKernel`; `https://github.com/f-t-s/nearLinKernel`]
Reason 3: the apparent keyword match is explained by “near-linear computational complexity” plus mathematical kernel matrices, making the repository name a lexical false positive. [`https://github.com/f-t-s/nearLinKernel`]
Reason 4: the code's reusable ideas are generic sparse-linear-algebra techniques, not transferable NearLink protocol or driver concepts. [`https://github.com/f-t-s/nearLinKernel`; `.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:197-204`]
Reason 5: the repository does not advance any current research-direction item, including SSAP, DLI, HADM, USB transport, or WS73 bring-up. [`.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:31-64`; `https://github.com/f-t-s/nearLinKernel`]
Recommended tracker label: false positive / name collision, with no follow-up inspection unless a separate numerical-linear-algebra need arises. [`https://github.com/f-t-s/nearLinKernel`; `.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:6-8`]

## Summary

1. `https://github.com/f-t-s/nearLinKernel` is a Julia numerical-experiments repository, not a NearLink implementation.
2. Its README ties the code to a paper on compression, inversion, and approximate PCA of dense kernel matrices.
3. The name collision comes from “near-linear computational complexity” and mathematical kernel matrices, not HiSilicon NearLink.
4. The code covers covariance functions, sparse Cholesky factorization, elimination ordering, iterative preconditioning, diagnostics, and plotting.
5. It has no HiSilicon, WS63/WS73, SLE, SSAP, DLI, HADM, USB, firmware, radio, or Linux-driver content.
6. Its generic numerical concepts have at most indirect reuse value and do not transfer to the current WS73 host/dongle work.
7. `RESEARCH-DIRECTIONS.md` and `COMMUNITY-PROJECTS.md` focus on protocol, transport, firmware, and vendor-SDK knowledge that this repository does not provide.
8. The repository should be excluded from future NearLink harvesting as a false positive.
9. Output path: `.scratch/nearlink-driver/lab-notes/NEW-NEARLINKERNEL-CLASSIFICATION.md`
