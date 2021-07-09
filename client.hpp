#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <set>
#include <string>
#include <cstdlib>

#include "json.hpp"
#include "httplib.h"

using json = nlohmann::json;

struct filebrowser_client{
    httplib::Client cli;
    httplib::Headers default_headers;

    json new_user_template;

    std::string token;


    filebrowser_client(const std::string& server, const json &tmpl) :
        cli(server.c_str()),
        new_user_template(tmpl)
        {}

    void login(const std::string &username, const std::string &password){
        json req;
        req["username"] = username;
        req["password"] = password;
        req["recaptcha"] = "";

        auto res = cli.Post("/api/login", req.dump(), "application/json");

        if (res == nullptr || res->status < 200 || res->status >= 300) {
            fprintf(stderr,
                "Could not login as %s, status code: %d\n",
                username.c_str(),
                res == nullptr ? 0 : res->status
            );
            exit(EXIT_FAILURE);
        }
        token = res->body;

        default_headers = {
            {"X-Auth", token}
        };
    }

    std::set<std::string> get_user_list(bool &ok){
        std::set<std::string> result;
        ok = true;
        auto res = cli.Get("/api/users", default_headers);

        if (res == nullptr || res->status < 200 || res->status >= 300) {
            ok = false;
            return result;
        }

        json obj = json::parse(res->body);
        for (const auto user:obj) {
            result.insert(user["username"].get<std::string>());
        }

        return result;
    }

    bool create_new_user(
        const std::string &username, const std::string &password
        )
    {
        bool ok = true;

        json data = new_user_template;
        data["data"]["username"] = username;
        data["data"]["password"] = password;
        data["data"]["scope"] = "./users/" + username;

        auto res = cli.Post("/api/users",
                        default_headers,
                        data.dump(),
                        "application/json"
        );

        if (res == nullptr || res->status < 200 || res->status >= 300) {
            return false;
        }

        return true;
    }
};

#endif