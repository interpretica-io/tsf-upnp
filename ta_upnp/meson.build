# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# External TE library (see TE_EXT_REPO): agent-side UPnP control point
# over libupnp (pupnp). The agent and its RPC server both link it.

headers += files(
    'ta_upnp.h',
)

sources += files(
    'ta_upnp.c',
)

te_libs += [
    'logger_core',
    'tools',
]

deps += [
    dependency('libupnp'),
    dependency('threads'),
]
