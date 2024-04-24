// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>
#include <string>

class PrometheusExporterHandler {
public:
	virtual std::string OnPrometheusExporterRequest() = 0;
	virtual void OnPrometheusExporterError(std::exception_ptr error) noexcept = 0;
};
