#include "surf_db.h"
#include "surf/option/surf_option.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "vendor/sql_mm/src/public/sqlite_mm.h"
#include "vendor/sql_mm/src/public/mysql_mm.h"

using namespace Surf::Database;

void SurfDatabaseService::SetupDatabase()
{
	KeyValues *config = SurfOptionService::GetOptionKV("db");
	if (!config)
	{
		SURF_LOG_INFO(LogChannel::DB, "No database config detected.\n");
		return;
	}
	ISQLInterface *sqlInterface = (ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr);
	if (!sqlInterface)
	{
		SURF_LOG_INFO(LogChannel::DB, "Database plugin not found. Local database is disabled.\n");
		return;
	}
	const char *driver = config->GetString("driver");
	if (!V_stricmp(driver, "sqlite"))
	{
		SQLiteConnectionInfo info;
		char path[MAX_PATH];
		V_snprintf(path, sizeof(path), "addons/cs2surf/data/%s.sqlite3", config->GetString("database"));
		info.database = path;
		databaseConnection = sqlInterface->GetSQLiteClient()->CreateSQLiteConnection(info);
		databaseType = DatabaseType::SQLite;
	}
	else if (!V_stricmp(driver, "mysql"))
	{
		MySQLConnectionInfo info = {config->GetString("host"),     config->GetString("user"),    config->GetString("pass"),
									config->GetString("database"), config->GetInt("port", 3306), config->GetInt("timeout", 60)};
		databaseConnection = sqlInterface->GetMySQLClient()->CreateMySQLConnection(info);
		databaseType = DatabaseType::MySQL;
	}
	else
	{
		SURF_LOG_INFO(LogChannel::DB, "No database config detected.\n");
	}
	databaseConnection->Connect(OnDatabaseConnected);
}

void SurfDatabaseService::OnDatabaseConnected(bool connect)
{
	if (connect)
	{
		SURF_LOG_INFO(LogChannel::DB, "LocalDB connected.\n");
		SurfDatabaseService::RunMigrations();
	}
	else
	{
		SURF_LOG_WARN(LogChannel::DB, "Failed to connect\n");
		// make sure to properly destroy the connection
		databaseConnection->Destroy();
		databaseConnection = nullptr;
	}
	return;
}
