// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * This header enables or disables certain features of the translation
 * client.  More specifically, it can be used to eliminate
 * #TranslateRequest and #TranslateResponse attributes.
 */

#pragma once

#define TRANSLATION_ENABLE_CACHE 0
#define TRANSLATION_ENABLE_WANT 0
#define TRANSLATION_ENABLE_EXPAND 0
#define TRANSLATION_ENABLE_SESSION 0
#define TRANSLATION_ENABLE_HTTP 0
#define TRANSLATION_ENABLE_WIDGET 0
#define TRANSLATION_ENABLE_RADDRESS 0
#define TRANSLATION_ENABLE_TRANSFORMATION 0
#define TRANSLATION_ENABLE_EXECUTE 0
#define TRANSLATION_ENABLE_SPAWN 0
#define TRANSLATION_ENABLE_LOGIN 0
