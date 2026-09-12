---
type: harvest
title: NEW-NEARLINKERNEL-CLASSIFICATION
language: en
created: 2026-09-12
tags: []
---

# NEW-NEARLINKERNEL-CLASSIFICATION

## Scope and inspection boundary

This report classifies the locally inspected repository `/mnt/hdd/nearlink-stuff/nearLinKernel` for possible relevance to HiSilicon NearLink/SparkLink work.
The inspection was read-only and local; no network access, build, test execution, or hardware interaction was performed.
The repository README describes the tree as code used for numerical examples in a paper titled “Compression, inversion, and approximate PCA of dense kernel matrices at near-linear computational complexity.” [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-3`]
The README links the paper rather than describing a wireless product, driver, SDK, or protocol implementation. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:4-5`]

## Executive classification

**Classification: name collision, not a HiSilicon NearLink/SparkLink project.**
The repository name appears to combine “near-linear” with “kernel,” matching the paper title's “near-linear computational complexity” and “dense kernel matrices,” rather than the NearLink radio technology. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-3`]
The inspected code contains numerical-linear-algebra modules and experiment scripts, not a Linux kernel driver or communications stack. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:1-18`]
No inspected file mentions HiSilicon, WS63, WS73, SparkLink, SLE, SSAP, HCI, USB transport, radio firmware, or a dongle. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:1-18`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
The only “kernel” concept present is a mathematical covariance/kernel-matrix concept used by the numerical experiments. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:134-163`]
The only “near-linear” concept present is the complexity claim in the paper title, not a NearLink protocol or device family. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-3`]
There is no evidence of a Linux kernel module, kernel-space API, device probe, URB handling, character device, ioctl surface, or hardware register access. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:21-36`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Factorization.jl:101-157`]
There is no evidence of a wireless protocol implementation, packet parser, state machine, link manager, security layer, or RF configuration. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:1-18`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
There is no evidence of vendor SDK integration, firmware packaging, board bring-up, serial flashing, or device-specific build instructions. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_N.jl:1-5`]
The appropriate harvesting decision is to exclude this repository from future NearLink harvesting as a false positive caused by the similar name. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:6-8`]

## Complete tracked-file inventory

The tracked non-Git inventory consists of one Markdown README and nineteen Julia source files; each entry below is represented in the inspected tree. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:1`]
1. `/mnt/hdd/nearlink-stuff/nearLinKernel/README.md` — project description and paper link. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`]
2. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl` — Cauchy-kernel scaling experiment. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:1-4`]
3. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/GenerateHighDimPoints.jl` — synthetic ellipse, spiral, and mixed-point generators. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/GenerateHighDimPoints.jl:1-5`]
4. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_highDim_vary_rho.jl` — high-dimensional Matérn experiment. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_highDim_vary_rho.jl:1-5`]
5. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_dz.jl` — Matérn experiment varying a z-axis deformation parameter. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_dz.jl:1-5`]
6. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_N.jl` — Matérn experiments varying problem size in two and three dimensions. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_N.jl:1-5`]
7. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_nu.jl` — Matérn experiment varying smoothness `nu`. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_nu.jl:1-5`]
8. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_rho.jl` — Matérn experiment varying the sparsity/oversampling parameter `rho`. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_rho.jl:1-5`]
9. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotBoundaryEffects.jl` — boundary-conditioning and Schur-complement visualization. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotBoundaryEffects.jl:1-12`]
10. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/plotMaternKernel.jl` — Matérn function and spectrum plots. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/plotMaternKernel.jl:1-8`]
11. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotOrderings.jl` — ordering and sparse-factor visualization. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotOrderings.jl:1-10`]
12. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotPCA.jl` — PCA-versus-Cholesky approximation plot. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotPCA.jl:1-14`]
13. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotSigmaEffect.jl` — nugget/sigma effect on covariance and inverse factors. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotSigmaEffect.jl:1-6`]
14. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Utils.jl` — Laplace-grid and boundary-point utilities. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Utils.jl:1-4`]
15. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl` — covariance-function definitions. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-4`]
16. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl` — approximation-error, scatter, and rank diagnostics. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:1-8`]
17. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Factorization.jl` — incomplete Cholesky implementation. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Factorization.jl:1-5`]
18. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/MutHeap.jl` — mutable maximum heap implementation. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/MutHeap.jl:1-12`]
19. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl` — sparse ordering and “daycare” data structure. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl:1-8`]
20. `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl` — main sparse-operator module and public API. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:1-18`]

## Language, build, and runtime inventory

The implementation language is Julia, as shown by the `.jl` files, `module` declaration, `function` definitions, and Julia package imports. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:2-8`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-4`]
The repository is not a C, C++, Rust, Python, or Go communications implementation; the tracked source set is Julia plus the Markdown README. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:2-8`]
The main module imports `IterativeSolvers`, `SparseArrays`, `Distributions`, `Random`, `LinearAlgebra`, and `Base.Threads`, which identifies a numerical-computing runtime rather than a driver runtime. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:3-8`]
The covariance module imports `SpecialFunctions` and defines Matérn, Gaussian, exponential, inverse-multiquadratic, rational-quadratic, and Cauchy functions. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
The experiment scripts include the main module, covariance functions, and diagnostics, showing that they are executable research scripts rather than installable protocol components. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:1-5`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_rho.jl:1-5`]
The scripts use plotting and typesetting packages including `Plots`, `PyPlot`, `TexTables`, and `LaTeXStrings`. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:6-15`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotBoundaryEffects.jl:1-12`]
The high-dimensional data generator uses `Rotations` and `Distributions` to create synthetic point clouds. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/GenerateHighDimPoints.jl:1-5`]
The sparse-ordering implementation additionally uses `StaticArrays` and `LinearAlgebra`. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl:1-4`]
The code contains no conventional build manifest, Makefile, CMakeLists file, package project file, or shell entrypoint in the inspected tracked set; its entrypoints are top-level Julia scripts and included modules. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:1-5`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:10-12`]
The scripts write LaTeX tables under `./out/`, for example `vary_rho_cauchy...tex`, `vary_rho_matern...tex`, and `vary_N_matern...tex`. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:92-94`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_rho.jl:172-174`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_N.jl:122-124`]
The plotting scripts write PDFs, PNGs, and JPEGs under `../figures/`, showing a paper-figure generation workflow. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotBoundaryEffects.jl:81-96`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/plotMaternKernel.jl:25-61`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotOrderings.jl:38-68`]
The code uses multithreading imports, but the inspected files do not define a device-service process, daemon, kernel thread, or hardware interrupt path. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:8`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:21-36`]
The experiments are large-scale numerical workloads: several scripts set `N = 1000000`, use 50 repetitions, and draw 500000 diagnostic samples. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:19-22`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_N.jl:26-30`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_rho.jl:26-32`]
The runtime therefore appears to be an offline Julia research environment with plotting backends, not a real-time or embedded runtime. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:3-8`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotBoundaryEffects.jl:14-20`]

## Protocol and domain inventory

The domain is numerical linear algebra for dense kernel matrices, specifically compression, inversion, and approximate PCA. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-3`]
The central abstraction is `AbstractSpelMat`, described as an operator represented by a sparse Cholesky factorization. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:21-24`]
`SpelMat` stores the number of degrees of freedom, nonzero count, elimination permutation, reverse permutation, and a sparse upper-triangular factor. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:24-36`]
Its constructor accepts a `d × N` array of points and an accuracy factor `rho`, then calls `sortSparse` to construct the sparsity pattern. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:39-46`]
The constructor builds a Julia `SparseMatrixCSC`, transposes it, and returns the sparse operator. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:52-65`]
Matrix conversion reconstructs a dense matrix as `U' * U` and applies the reverse elimination ordering. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:67-70`]
The module overloads logarithmic determinant, multiplication, left division, right division, indexing, and related linear-algebra operations. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:73-131`]
`setCovStat!` fills sparse-factor entries from pairwise point distances, maps a supplied covariance function over those distances, applies a scale `eta`, and optionally adds a nugget `sigma`. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:134-163`]
The code explicitly warns that larger nugget terms degrade the approximation property and recommends `SpelMatIter` for large nuggets. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:134-139`]
`SpelMatIter` stores both the compressed factor and a preconditioning factor, plus `sigma` and tolerance fields. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:167-189`]
The iterative constructor creates a second sparse CSC preconditioner from the same sparsity pattern. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:191-225`]
Its covariance setup copies values into both the main factor and preconditioner and runs incomplete Cholesky on both. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:236-265`]
The iterative operator adds the nugget term directly to matrix-vector products. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:268-285`]
A wrapper applies the preconditioner before and after the compressed operator, while conjugate gradients solve the preconditioned system. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:288-329`]
The diagnostics module compares exact covariance values with approximated entries and returns true, approximate, error, and distance vectors. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:7-31`]
A second diagnostic path excludes points near a boundary, which is useful for interior-error experiments. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:33-66`]
The diagnostics also estimate Frobenius and relative Frobenius errors over repeated random samples. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:109-141`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:150-181`]
The factorization module implements an in-place incomplete Cholesky routine over sparse-column pointers and row indices. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Factorization.jl:99-157`]
The ordering module implements a mutable maximum heap with a lookup array for node positions. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/MutHeap.jl:9-13`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/MutHeap.jl:97-109`]
The same module implements heap update and move-down operations used to maintain nearest-parent distances. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/MutHeap.jl:53-90`]
`SortSparse.jl` defines a “daycare” structure that tracks descendants of each elimination parent in buffered sparse columns. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl:3-25`]
Its child-selection logic tests a distance threshold based on `lengthscale * rho`, updates the heap, and may update a preferred parent. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl:60-123`]
The Euclidean wrapper computes squared distances by summing coordinate differences, then returns column pointers, row values, and permutations. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl:185-209`]
The experiment scripts vary `rho`, `N`, `nu`, and a deformation parameter `dz`, confirming that the repository studies algorithmic scaling and approximation quality. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Cauchy_Vary_rho.jl:14-22`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_N.jl:24-30`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_nu.jl:17-26`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_dz.jl:28-36`]
The scripts repeatedly construct `SpelMat`, fill covariance values, time sorting and factorization, measure rank and sparsity, and estimate approximation error. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_rho.jl:72-92`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/Matern_Vary_N.jl:56-80`]
The boundary-effects script forms dense covariance matrices and a Schur complement after conditioning on boundary points. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotBoundaryEffects.jl:18-45`]
The PCA script compares truncated Cholesky and eigenvalue approximations against a dense Matérn covariance matrix. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotPCA.jl:20-53`]
The sigma-effect script studies `ExpKOrd + sigma * I` and the corresponding inverse, again in a numerical-analysis setting. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotSigmaEffect.jl:8-25`]
No file defines packet framing, service discovery, connection management, SSAP properties, DLI commands, HADM ranging, PHY parameters, or USB endpoint behavior. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:13-18`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]

## Reusable concepts and transfer limits

The sparse Cholesky representation is a reusable numerical concept for low-rank or sparse approximation, but it is not a communications abstraction. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:21-36`]
The elimination-ordering and sparsity-pattern construction could inspire generic preprocessing or nearest-neighbor indexing, but the implementation is tightly coupled to Julia sparse matrices and Euclidean point data. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl:125-181`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:39-65`]
The covariance-function interface is a clean example of passing a mathematical kernel as a callable object and applying it to sparse distances. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:134-151`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
The nugget/preconditioner split is a reusable numerical idea for stabilizing ill-conditioned systems, not a radio-link flow-control mechanism. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:167-189`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:236-265`]
The conjugate-gradient wrapper demonstrates a preconditioned iterative solve around a compressed operator. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:288-329`]
The mutable heap with a reverse lookup table is a reusable data-structure pattern for updating priorities by identifier. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/MutHeap.jl:9-13`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/MutHeap.jl:97-109`]
The “daycare” structure is an application-specific buffered sparse-column representation for elimination descendants. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/SortSparse.jl:3-25`]
The diagnostic functions provide a reusable pattern for sampling exact versus approximate entries and reporting absolute or relative Frobenius error. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:7-31`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:109-141`]
The synthetic generators provide reusable patterns for controlled low-dimensional manifolds embedded in higher-dimensional spaces. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/GenerateHighDimPoints.jl:11-38`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/GenerateHighDimPoints.jl:70-84`]
The plotting scripts provide reusable paper-figure patterns for covariance curves, spectra, ordering heat maps, and approximation-error comparisons. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/plotMaternKernel.jl:15-61`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotOrderings.jl:29-68`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/PlotPCA.jl:46-80`]
Direct code reuse in the NearLink driver effort would be poor because the source is Julia research code while the target effort is a Linux host/dongle protocol and driver project. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:2-8`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`]
The numerical concepts do not supply wire-format knowledge, device enumeration, USB transfers, firmware state machines, or SSAP semantics needed by the WS73 work. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:13-18`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`]
At most, the repository could be cited as an example of sparse-matrix experimentation if a future NearLink task independently needs numerical modeling; it should not be harvested as a NearLink implementation source. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:109-141`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:197-204`]

## Comparison with RESEARCH-DIRECTIONS.md

`RESEARCH-DIRECTIONS.md` is explicitly centered on WS63/WS73 HiSilicon work, including WiFi/SLE coexistence, dual-dongle connection, SSAP, DLI, HADM ranging, and the WS73 USB driver skeleton. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:6-16`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:31-44`]
That document treats the WS73 path as a five-endpoint HCC USB design with a proposed `/dev/ws73hci` character device and firmware-download state machine. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`]
It also records protocol-level work on SSAP, DLI opcodes, ACB credit control, HADM commands, and USB transport contracts. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:51-64`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:194-211`]
By contrast, `nearLinKernel` contains no device-specific ABI, no endpoint management, no command/event opcodes, and no transport framing. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:13-18`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
The research directions value OpenSparklink and WS63 sources for byte-level protocol cross-checking, whereas `nearLinKernel` provides only mathematical covariance and factorization code. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:51-64`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Diagnostics.jl:7-31`]
The only superficial similarity is the word “kernel”: the NearLink documents use kernel in the Linux-driver sense or not at all, while this repository uses kernel in the statistical/covariance-matrix sense. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:41-44`; `/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-3`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
No concept from `nearLinKernel` maps directly to the NearLink harvesting priorities of SSAP behavior, DLI transport, USB contracts, HADM ranging, or vendor firmware bring-up. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:31-64`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:13-18`]

## Comparison with COMMUNITY-PROJECTS.md

`COMMUNITY-PROJECTS.md` defines useful NearLink projects as chip-side or host-side SLE implementations, protocol documentation, firmware tooling, or vendor SDK references. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:6-8`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:16-32`]
Its high-value examples include NearLinkSLE for SSAP call order, `sle_measure_sdk` for PHY/MCS/CI and credit-gated sending, and the BS21E SDK for the SLE-Link wire specification. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:70-88`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:122-129`]
That report explicitly distinguishes transferable protocol/API knowledge from code that cannot be reused because it runs inside vendor chip SDKs. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:197-204`]
`nearLinKernel` fits neither side of that distinction: it is not a chip-side SLE sample, not a host-side SLE stack, not firmware tooling, and not a vendor SDK reference. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:1-18`]
Its Julia covariance code has less NearLink relevance than even the community report's “edge” projects, because the edge projects at least demonstrate a serial bridge or product topology involving SLE hardware. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:37-49`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
The community report's reusable concepts are SSAP sequencing, CCCD notification behavior, ACB flow control, PHY tuning, frame aggregation, and SLE-Link TLV/CRC framing; none is implemented in `nearLinKernel`. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:143-160`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:13-18`]
The community report also identifies the WS73 vendor SDK as a confirmation baseline rather than a source of new protocol knowledge; `nearLinKernel` is even further removed because it has no HiSilicon SDK tree or device code. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:131-134`; `/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`]

## Harvesting decision

**Exclude `nearLinKernel` from future NearLink harvesting.**
Reason 1: the repository's own README identifies a numerical-examples paper, not a NearLink, SparkLink, HiSilicon, or Linux-driver project. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`]
Reason 2: the complete source inventory is Julia numerical analysis code, with no wireless, USB, kernel-driver, firmware, or protocol files. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:1-18`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/CovFuncs.jl:1-28`]
Reason 3: the apparent keyword match is explained by “near-linear computational complexity” plus mathematical kernel matrices, making the repository name a lexical false positive. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-3`]
Reason 4: the code's reusable ideas are generic sparse-linear-algebra techniques, not transferable NearLink protocol or driver concepts. [`/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:21-36`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:197-204`]
Reason 5: the repository does not advance any current research-direction item, including SSAP, DLI, HADM, USB transport, or WS73 bring-up. [`/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/RESEARCH-DIRECTIONS.md:31-64`; `/mnt/hdd/nearlink-stuff/nearLinKernel/src/spelliptic/Spelliptic.jl:13-18`]
Recommended tracker label: false positive / name collision, with no follow-up inspection unless a separate numerical-linear-algebra need arises. [`/mnt/hdd/nearlink-stuff/nearLinKernel/README.md:1-5`; `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md:6-8`]

## Summary

1. `/mnt/hdd/nearlink-stuff/nearLinKernel` is a Julia numerical-experiments repository, not a NearLink implementation.
2. Its README ties the code to a paper on compression, inversion, and approximate PCA of dense kernel matrices.
3. The name collision comes from “near-linear computational complexity” and mathematical kernel matrices, not HiSilicon NearLink.
4. The code covers covariance functions, sparse Cholesky factorization, elimination ordering, iterative preconditioning, diagnostics, and plotting.
5. It has no HiSilicon, WS63/WS73, SLE, SSAP, DLI, HADM, USB, firmware, radio, or Linux-driver content.
6. Its generic numerical concepts have at most indirect reuse value and do not transfer to the current WS73 host/dongle work.
7. `RESEARCH-DIRECTIONS.md` and `COMMUNITY-PROJECTS.md` focus on protocol, transport, firmware, and vendor-SDK knowledge that this repository does not provide.
8. The repository should be excluded from future NearLink harvesting as a false positive.
9. Output path: `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/NEW-NEARLINKERNEL-CLASSIFICATION.md`
