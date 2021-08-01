/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

struct appdata_t;
struct project_t;

std::unique_ptr<project_t> project_select(appdata_t &appdata);
