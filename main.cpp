#include <cstdlib>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>

#include <set>
#include <regex>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "config_reader.hpp"
#include "client.hpp"

static bool
match_directory(const char *dname, std::string& serial_no, std::string& model)
{
	static const std::regex str_expr("([0-9]+)-([a-zA-Z0-9]+)");
	const uint32_t len = strlen(dname);

	if (std::regex_match(dname, dname + len, str_expr)) {
		auto ptr = std::find(dname, dname + len, '-');
		serial_no = std::string(dname, ptr);
		model = std::string(ptr + 1, dname + len);

		return true;
	} else {
		return false;
	}
}

static bool
create_user(
	const std::string &username,
	filebrowser_client *client, const config &cfg)
{
	if (client->create_new_user(username, cfg.new_user_password)) {
		fprintf(stderr, "New user %s created\n",
			username.c_str()
		);
		return true;
	} else {
		fprintf(stderr, "User %s not created\n",
			username.c_str()
		);
		return false;
	}
}

static void
handle_events(int fd, int wd, const config& cfg, filebrowser_client* client)
{
	char buf[4096]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;
	int i;
	ssize_t len;
	char *ptr;

	std::string serial_no;
	std::string model;

	/* Loop while events can be read from inotify file descriptor. */

	for (;;) {

		/* Read some events. */

		len = read(fd, buf, sizeof buf);
		if (len == -1 && errno != EAGAIN) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		/* If the nonblocking read() found no events to read, then
		   it returns -1 with errno set to EAGAIN. In that case,
		   we exit the loop. */

		if (len <= 0)
			break;

		/* Loop over all events in the buffer */

		for (ptr = buf; ptr < buf + len;
				ptr += sizeof(struct inotify_event) + event->len) {

			event = (const struct inotify_event *) ptr;

			// Ignore files. we are only interested in directories

			if ((event->mask & IN_ISDIR) == 0)
				continue;
			
			assert(event->mask & IN_CREATE);
			assert(event->len);
			assert(event->name[0] != '\0');

			// Check if we should create a user for this directory

			if (match_directory(event->name, serial_no, model)) {
				create_user(serial_no + "-" + model, client, cfg);
			} else {
				continue;
			}
		}
	}
}

static void create_users_from_dirs(
	filebrowser_client *client, const config &cfg
	)
{
	bool ok = true;

	DIR *d;
	dirent *dir;
	std::set<std::string> users;
	std::string serial_no, model;
	std::string result;

	d = opendir(cfg.watch_dir.c_str());
	if (d == NULL) {
		perror("opendir");
		exit(EXIT_FAILURE);
	}

	users = client->get_user_list(ok);
	if (!ok) {
		fprintf(stderr, "Failed to get user list\n");
		exit(EXIT_FAILURE);
	}

	while ((dir = readdir(d)) != NULL) {
		if (match_directory(dir->d_name, serial_no, model)) {
			result = serial_no + "-" + model;
			if (users.count(result)) {
				continue;
			}
			if (!create_user(result, client, cfg)) {
				fprintf(stderr, "Failed to create username\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

int main(const int argc, const char **argv)
{
	int fd, i, poll_num;
	int wd;
	char buf;
	
	pollfd fds;
	nfds_t nfds;

	config cfg;
	filebrowser_client *client = nullptr;
	
	// Read configuration from argc, argv

	cfg.read(argc, argv);

	client = new filebrowser_client(cfg.server_addr, cfg.new_user_template);
	client->login(cfg.admin_user, cfg.admin_password);

	create_users_from_dirs(client, cfg);

	/* Create the file descriptor for accessing the inotify API */

	fd = inotify_init1(IN_NONBLOCK);
	if (fd == -1) {
		perror("inotify_init1");
		exit(EXIT_FAILURE);
	}

	/* Mark directory for event
	   - file was created */

	wd = inotify_add_watch(fd, cfg.watch_dir.c_str(), IN_CREATE);
	if (wd == -1) {
		fprintf(stderr, "Cannot watch '%s': %s\n", argv[i], strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Prepare for polling */

	nfds = 1;

	/* Inotify input */

	fds.fd = fd;
	fds.events = POLLIN;

	/* Wait for events and/or terminal input */

	fprintf(stderr, "Listening for events at %s\n", cfg.watch_dir.c_str());
	while (1) {
		poll_num = poll(&fds, nfds, -1);
		
		// Stop if program is interrupted

		if (poll_num == -1) {
			if (errno == EINTR)
				break;
			perror("poll");
			exit(EXIT_FAILURE);
		}

		if (poll_num > 0) {
			if (fds.revents & POLLIN) {

				/* Inotify events are available */

				handle_events(fd, wd, cfg, client);
			}
		}
	}

	/* Close inotify file descriptor */

	close(fd);

	exit(EXIT_SUCCESS);
}

