#include "net/log/Datagram.hxx"
#include "net/log/Parser.hxx"
#include "net/log/OneLine.hxx"

extern "C" {
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const auto input = std::as_bytes(std::span{data, size});

	try {
		const auto d = Net::Log::ParseDatagram(input);
		char output[256];
		FormatOneLine(output, sizeof(output), d, true);
	} catch (Net::Log::ProtocolError) {
	}

	return 0;
}
