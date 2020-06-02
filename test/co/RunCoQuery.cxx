#include "co/InvokeTask.hxx"
#include "net/ToString.hxx"
#include "pg/Stock.hxx"
#include "pg/CoStockQuery.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"

#include <cstdio>
#include <cstdlib>
#include <memory>

struct Instance final {
	EventLoop event_loop;
	Pg::Stock db;

	std::exception_ptr error;

	Instance(const char *conninfo, const char *schema)
		:db(event_loop, conninfo, schema, 4, 1)
	{
	}

	void Shutdown() noexcept {
		db.Shutdown();
	}

	void OnCompletion(std::exception_ptr _error) noexcept {
		error = std::move(_error);
		Shutdown();
	}
};

static void
PrintResult(const Pg::Result &result) noexcept
{
	const unsigned n_columns = result.GetColumnCount();
	for (unsigned i = 0; i < n_columns; ++i)
		printf("%s\t", result.GetColumnName(i));
	printf("\n");

	for (const auto &row : result) {
		for (unsigned i = 0; i < n_columns; ++i)
			printf("%s\t", row.GetValue(i));
		printf("\n");
	}
}

static Co::InvokeTask
Run(Pg::Stock &db, const char *sql)
{
	auto result = co_await Pg::CoStockQuery(db, sql);
	if (result.IsError())
		throw Pg::Error(std::move(result));

	PrintResult(result);
}

int
main(int argc, char **argv) noexcept
try {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s CONNINFO SQL\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *const conninfo = argv[1];
	const char *const sql = argv[2];

	Instance instance(conninfo, "");

	auto task = Run(instance.db, sql);
	task.OnCompletion(BIND_METHOD(instance, &Instance::OnCompletion));

	instance.event_loop.Dispatch();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
