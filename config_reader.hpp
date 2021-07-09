#ifndef CONFIG_READER_HPP
#define CONFIG_READER_HPP

#include <string>
#include <fstream>
#include <iostream>

#include "json.hpp"

using json = nlohmann::json;

struct config {
	json raw;
	json new_user_template;

	std::string watch_dir;
	std::string server_addr;

	std::string admin_user;
	std::string admin_password;
	std::string new_user_password;

	void read(const std::string &config_path){
		std::ifstream config_file(config_path);
		config_file >> raw;

		new_user_template = raw["new_user_template"];

		watch_dir = raw["watch_dir"];
		server_addr = raw["server_address"];
		admin_user = raw["admin_username"];
		admin_password = raw["admin_password"];
		new_user_password = raw["new_user_password"];
	}
	void read(const int argc, const char **argv){
		std::string config_file;

		if (argc < 2) {
			config_file = "/etc/filewatcher/config.json";
		} else {
			if(argc > 2){
			   printf("Usage: %s CONFIG_FILE\n", argv[0]);
			   exit(EXIT_FAILURE);
			}
			config_file = argv[1];
			try{
				read(config_file);
			} catch(std::exception &e){
				std::cerr << e.what() << std::endl;
				exit(EXIT_FAILURE);
			}
		}

		if (watch_dir == "") {
			fprintf(stderr, "'watch_dir' string is empty\n");
			exit(EXIT_FAILURE);
		}
	}
};

#endif
