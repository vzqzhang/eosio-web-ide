#include <eosio/eosio.hpp>

// Message table
struct [[eosio::table("message"), eosio::contract("talk")]] message {
    uint64_t    id       = {}; // Non-0
    uint64_t    reply_to = {}; // Non-0 if this is a reply
    eosio::name user     = {};
    std::string content  = {};
    std::set<eosio::name> liked_by  = {};

    uint64_t primary_key() const { return id; }
    uint64_t get_reply_to() const { return reply_to; }
};

// Like table
struct [[eosio::table("likepost"), eosio::contract("talk")]] likepost {
    uint64_t    id       = {}; // Non-0
    std::set<eosio::name> liked_by  = {};

    uint64_t primary_key() const { return id; }
};

using message_table = eosio::multi_index<
    "message"_n, message, eosio::indexed_by<"by.reply.to"_n, eosio::const_mem_fun<message, uint64_t, &message::get_reply_to>>>;
using like_table = eosio::multi_index<
    "likepost"_n, likepost, eosio::indexed_by<"by.post.id"_n, eosio::const_mem_fun<likepost, uint64_t, &likepost::primary_key>>>;

// The contract
class talk : eosio::contract {
  public:
    // Use contract's constructor
    using contract::contract;

    // Post a message
    [[eosio::action]] void post(uint64_t id, uint64_t reply_to, eosio::name user, const std::string& content) {
        message_table table{get_self(), 0};

        // Check user
        require_auth(user);

        // Check reply_to exists
        if (reply_to)
            table.get(reply_to);

        // Create an ID if user didn't specify one
        eosio::check(id < 1'000'000'000ull, "user-specified id is too big");
        if (!id)
            id = std::max(table.available_primary_key(), 1'000'000'000ull);

        // Record the message
        table.emplace(get_self(), [&](auto& message) {
            message.id       = id;
            message.reply_to = reply_to;
            message.user     = user;
            message.content  = content;
        });
    }

    //Like a post
    [[eosio::action]] void like(uint64_t id, eosio::name user) {
        message_table msg_table{get_self(), 0};
        like_table lk_table{get_self(), 0};

        // Check user
        require_auth(user);

        // Check message id exists
        msg_table.get(id);

        auto lk = lk_table.find(id);
        if (lk == lk_table.end()) {
            std::set<eosio::name> liked_by  = {};
            liked_by.insert(user);
            lk_table.emplace(get_self(), [&](auto& like) {
                like.id       = id;
                like.liked_by = liked_by;
            });
        } else {
            std::set<eosio::name> liked_by  = lk->liked_by;
            auto ur = liked_by.find(user);
            if (ur != liked_by.end())
                return;

            liked_by.insert(user);
            lk_table.modify(lk, user, [&]( auto& like ) {
                like.liked_by = liked_by;
            });
        }
    }
};