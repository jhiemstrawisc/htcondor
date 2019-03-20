
#include "match_prefix.h"
#include "CondorError.h"
#include "condor_config.h"
#include "Regex.h"
#include "directory.h"

#if defined(HAVE_EXT_OPENSSL)

// The GCC_DIAG_OFF() disables warnings so that we can build on our
// -Werror platforms.  We have to undefine max and min because of
// Windows-related silliness.

GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#undef min
#undef max
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

#endif

#include <fstream>

namespace {

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s\n\n"
		"Lists all the tokens available to the current user.\n", argv0);
	exit(1);
}

bool printToken(const std::string &tokenfilename) {

	dprintf(D_SECURITY|D_FULLDEBUG, "TOKEN: Will use examine tokens found in %s.\n",
		tokenfilename.c_str());
	std::ifstream tokenfile(tokenfilename, std::ifstream::in);
	if (!tokenfile) {
		dprintf(D_ALWAYS, "Failed to open token file %s\n", tokenfilename.c_str());
		return false;
	}

	for (std::string line; std::getline(tokenfile, line); ) {
		line.erase(line.begin(),
			std::find_if(line.begin(),
				line.end(),
				[](int ch) {return !isspace(ch) && (ch != '\n');}));
		if (line.empty() || line[0] == '#') {
			continue;
		}
#if defined(HAVE_EXT_OPENSSL)
		try {
			auto decoded_jwt = jwt::decode(line);
			printf("Header: %s Payload: %s File: %s\n", decoded_jwt.get_header().c_str(),
				decoded_jwt.get_payload().c_str(),
				tokenfilename.c_str());
		} catch (std::exception) {
			dprintf(D_ALWAYS, "Failed to decode JWT in keyfile; ignoring.\n");
		}
#endif
	}
	return true;
}

bool
printAllTokens() {
	std::string dirpath;
	if (!param(dirpath, "SEC_TOKEN_DIRECTORY")) {
		MyString file_location;
		if (!find_user_file(file_location, "tokens.d", false)) {
			param(dirpath, "SEC_TOKEN_SYSTEM_DIRECTORY");
		} else {
			dirpath = file_location;
		}
	}
	dprintf(D_FULLDEBUG, "Looking for tokens in directory %s\n", dirpath.c_str());

	const char* _errstr;
	int _erroffset;
	std::string excludeRegex;
		// We simply fail invalid regex as the config subsys should have EXCEPT'd
		// in this case.
	if (!param(excludeRegex, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP")) {
		dprintf(D_FULLDEBUG, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP is unset");
		return false;
	}
	Regex excludeFilesRegex;   
	if (!excludeFilesRegex.compile(excludeRegex, &_errstr, &_erroffset)) {
		dprintf(D_FULLDEBUG, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP "
			"config parameter is not a valid "
			"regular expression.  Value: %s,  Error: %s",
			excludeRegex.c_str(), _errstr ? _errstr : "");
		return false;
	}
	if(!excludeFilesRegex.isInitialized() ) {
		dprintf(D_FULLDEBUG, "Failed to initialize exclude files regex.");
		return false;
	}

	Directory dir(dirpath.c_str());
	if (!dir.Rewind()) {
		dprintf(D_SECURITY, "Cannot open %s: %s (errno=%d)",
			dirpath.c_str(), strerror(errno), errno);
		return false;
	}

	const char *file;
	while ( (file = dir.Next()) ) {
		if (dir.IsDirectory()) {
			continue;
		}
		if(!excludeFilesRegex.match(file)) {
			printToken(dir.GetFullPath());
		} else {
			dprintf(D_FULLDEBUG|D_SECURITY, "Ignoring token file "
				"based on LOCAL_CONFIG_DIR_EXCLUDE_REGEXP: "
				"'%s'\n", dir.GetFullPath());
		}
	}
	return true;
}

}


int main(int argc, char *argv[]) {
#if !defined(HAVE_EXT_OPENSSL)
	fprintf(stderr, "Cannot list tokens on HTCondor build without OpenSSL\n");
	return 1;
#else
        for (int i = 1; i < argc; i++) {
		if(!strcmp(argv[i],"-debug")) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
		} else if (is_dash_arg_prefix(argv[i], "help", 1)) {
			print_usage(argv[0]);
			exit(1);
		} else {
			fprintf(stderr, "%s: Invalid command line argument: %s\n", argv[0], argv[i]);
			print_usage(argv[0]);
			exit(1);
		}
	}

	config();

	printAllTokens();
	return 0;
#endif
}