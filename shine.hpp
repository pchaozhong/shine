/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <utility>
#include <vector>
#include <eosiolib/crypto.h>
#include <eosiolib/types.hpp>
#include <eosiolib/token.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/contract.hpp>
#include <eosio.system/eosio.system.hpp>

// Configurable values
#define CURRENCY_PRECISION 4

#define GLOBAL_STAT_ID 0

#define REWARD_PRAISE_POSTED_WEIGHT 0.07
#define REWARD_VOTE_RECEIVED_WEIGHT 0.9
#define REWARD_VOTE_GIVEN_WEIGHT 0.03

// Macro
#define TABLE_VERSIONNED(X, VERSION) ::eosio::string_to_name(#X #VERSION)
#define TABLE(X) TABLE_VERSIONNED(X, m)

typedef checksum256 member_id;

using eos_currency = eosiosystem::contract<N(eosio)>::currency;
using eosio::asset;
using eosio::const_mem_fun;
using eosio::indexed_by;
using eosio::key256;
using std::string;

class shine : public eosio::contract
{
  public:
    shine(account_name contract_account)
        : eosio::contract(contract_account),
          praises(contract_account, contract_account),
          votes(contract_account, contract_account),
          stats(contract_account, contract_account),
          global_stats(contract_account, contract_account),
          rewards(contract_account, contract_account)
    {
    }

    //@abi action
    void addpraise(const member_id& author, const member_id& praisee, const string &memo);

    //@abi action
    void addvote(const uint64_t praise_id, const member_id& voter);

    //@abi action
    void calcrewards(const asset& pot);
  private:
    //@abi table praises i64
    struct praise
    {
        uint64_t id;
        member_id author;
        member_id praisee;
        string memo;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(praise, (id)(author)(praisee)(memo))
    };

    typedef eosio::multi_index<TABLE(praises), praise> praises_index;

    //@abi table votes i64
    struct vote
    {
        uint64_t id;
        uint64_t praise_id;
        member_id voter;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(vote, (id)(praise_id)(voter))
    };

    typedef eosio::multi_index<TABLE(votes), vote> votes_index;

    //@abi table memberstats i64
    /**
     * praise_posted - The praise count posted by the member
     * praise_vote_received - The amount of vote received for all praise posted by the member
     * vote_given_explicit - The amout of vote the member did on other member praises
     * vote_received_implicit - The amount of implicit vote (vote by the author of the praise) received
     * vote_received_explicit - The amount of explicit vote (voty by other members than author of praise) received
     */
    struct member_stat
    {
        uint64_t id;
        member_id member;
        uint64_t praise_posted;
        uint64_t praise_vote_received;
        uint64_t vote_given_explicit;
        uint64_t vote_received_implicit;
        uint64_t vote_received_explicit;

        member_stat() :
          praise_posted(0),
          praise_vote_received(0),
          vote_given_explicit(0),
          vote_received_implicit(0),
          vote_received_explicit(0)
        {}

        uint64_t primary_key() const { return id; }
        key256 by_member() const { return shine::compute_member_id_key(member); }

        EOSLIB_SERIALIZE(member_stat, (id)(member)(praise_posted)(praise_vote_received)(vote_given_explicit)(vote_received_implicit)(vote_received_explicit))
    };

    typedef eosio::multi_index<TABLE(memberstats), member_stat,
      indexed_by< N(member), const_mem_fun<member_stat, key256, &member_stat::by_member>>
    > member_stats_index;

    //@abi table global_stats i64
    struct global_stat
    {
        uint64_t id;
        uint64_t vote_implicit;
        uint64_t vote_explicit;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(global_stat, (id)(vote_implicit)(vote_explicit))
    };

    typedef eosio::multi_index<TABLE(globalstats), global_stat> global_stats_index;

    //@abi table rewards i64
    struct reward
    {
        uint64_t id;
        member_id member;
        asset amount_praise_posted;
        asset amount_vote_received;
        asset amount_vote_given;
        asset amount_total;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(reward, (id)(member)(amount_praise_posted)(amount_vote_received)(amount_vote_given)(amount_total))
    };

    typedef eosio::multi_index<TABLE(rewards), reward> rewards_index;

    static const asset_symbol EOS_SYMBOL;

    inline static asset double_to_asset(double amount) {
        return asset((uint64_t) (pow(10, CURRENCY_PRECISION) * amount), EOS_SYMBOL);
    }

    inline static double asset_to_double(const asset& asset) {
        return asset.amount / pow(10, CURRENCY_PRECISION);
    }

    static key256 compute_member_id_key(const member_id& member_id)
    {
        const uint64_t* p64 = reinterpret_cast<const uint64_t*>(&member_id);
        return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
    }

    static double compute_vote_given_weight(uint64_t vote_given) {
      if (vote_given <= 0) return 0;
      if (vote_given <= 1) return 1 / 3.0;
      if (vote_given <= 2) return 2 / 3.0;

      return 1.0;
    }

    double compute_vote_given_weighted_total() const;

    bool has_praise(const uint64_t praise_id) const {
        return praises.find(praise_id) != praises.end();
    }

    void update_global_stat(const std::function<void(global_stat&)> updater);

    void update_reward_for_member(
        const uint64_t id,
        const member_id& member,
        const std::function<void(reward&)> updater
    );

    void update_member_stat(
        const member_id& member,
        const std::function<void(member_stat&)> updater
    );

    praises_index praises;
    votes_index votes;
    member_stats_index stats;
    global_stats_index global_stats;
    rewards_index rewards;
};