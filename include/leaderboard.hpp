#pragma once
#include <algorithm>  // std::min
using namespace std;
#include "utils.hpp"
using namespace eosio;

struct Leaderboard {
  eosio::name _self;
  eosio::time_point_sec now = eosio::time_point_sec(eosio::current_time_point());
  donations::config_table _conf;
  donations::config lbconf;

  uint64_t round_id;

  Leaderboard(eosio::name _Self) : _conf(_Self, _Self.value) {
    _self = _Self;
    lbconf = _conf.get_or_create(_self, donations::config());
    round_id = get_round_id();
  }

  vector<rewards_data> generate_round_rewards(uint64_t round_id, donations::rounds round_data) {
    donations::leaderboard_table _leaderboard(_self, round_id);
    auto const self = _self;
    const auto& total = round_data.total_donated;
    const auto& mint_price = lbconf.mint_price_min;
    const auto& max_mint = lbconf.max_mint_per_round;
    auto by_score = _leaderboard.get_index<"byscore"_n>();
    donations::accounts_table _accounts(_self, _self.value);
    int allocated = 0;
    uint8_t rank = 1;
    auto price = mint_price;
    vector<rewards_data> rewards;
    for(auto ldbrd_itr = by_score.begin();
        ldbrd_itr != by_score.end() && allocated != lbconf.max_mint_per_round;
        ldbrd_itr++, rank++  //
    ) {
      const auto row = *ldbrd_itr;
      print("\nname: ", ldbrd_itr->donator, "\n");
      print("donated: ", ldbrd_itr->donated.to_string(), "\n");
      print("mint_price: ", price.to_string(), "\n");
      if(ldbrd_itr->donated < mint_price) continue;
      uint8_t remaining = max_mint - allocated;
      uint8_t mint_num = uint8_t(ldbrd_itr->donated.amount / price.amount);
      price += lbconf.mint_price_increase_by_rank;
      uint8_t to_mint = min(remaining, mint_num);
      print("to_mint: ", to_string(to_mint), "\n");
      // check(false, "stop");
      if(to_mint == 0) continue;
      allocated += to_mint;
      auto accts_itr = _accounts.find(ldbrd_itr->donator.value);
      // create account if this is the first time for this user to win
      if(accts_itr == _accounts.end()) {
        _accounts.emplace(_self, [&](donations::accounts& row) {
          row.account = ldbrd_itr->donator;
          row.bronze_unclaimed = to_mint;
        });
      } else {
        // add to their unclaimed balance
        _accounts.modify(accts_itr, _self, [&](donations::accounts& row) {
          row.bronze_unclaimed = u8add(to_mint, row.bronze_unclaimed);
        });
      }

      const auto rwd_dta = rewards_data {
        .donator = row.donator,
        .donated = row.donated,
        .score = row.score,
        .rank = rank,
        .bronze_nfts_awarded = to_mint
        
      };
      rewards.push_back(rwd_dta);
    };
    return rewards;
  };

  void addScore(eosio::name& donator, eosio::asset& donation) {
    if(disable_leaderboard()) {
      return;
    }

    donations::rounds_table _rounds(_self, _self.value);
    auto r_itr = _rounds.find(round_id);

    //leaderboard is scoped with round_id
    donations::leaderboard_table _leaderboard(_self, round_id);
    auto lb_itr = _leaderboard.find(donator.value);

    bool round_exists = r_itr != _rounds.end();
    bool has_donated = lb_itr != _leaderboard.end();

    uint64_t score = convert_donation_to_score(donation);

    round_exists ? modify_round(score, donation, _rounds, r_itr, has_donated) : emplace_round(score, donation, donator, _rounds);
    has_donated ? modify_leaderboard(score, donation, _leaderboard, lb_itr) : emplace_leaderboard(score, donation, donator, _leaderboard);
  }

 private:
  bool disable_leaderboard() {
    if(!lbconf.enabled) {
      //eosio::check(false, "Leaderboard disabled in config.");
      eosio::print("Leaderboard disabled in config.");
      return true;
    }
    if(lbconf.start_time > now) {
      int wait_seconds = lbconf.start_time.sec_since_epoch() - now.sec_since_epoch();
      //eosio::check(false, "Leaderboard not active. wait "+to_string(wait_seconds)+" seconds");
      eosio::print("Leaderboard not active. wait " + to_string(wait_seconds) + " seconds");
      return true;
    }
    return false;
  }

  uint64_t get_round_id() {
    int time_passed = (now.sec_since_epoch() - lbconf.start_time.sec_since_epoch());
    uint64_t id = floor(time_passed / lbconf.round_length_sec) + 1;
    // check(false, "current round:" + to_string(id));
    return id;
  }

  uint64_t convert_donation_to_score(eosio::asset& donation) {
    //bonus calculation = pv * Math.pow((1-r), n)
    int pv = donation.amount;
    int sec_passed = now.sec_since_epoch() - get_round_start_time().sec_since_epoch();
    if(sec_passed < lbconf.start_decay_after_sec) {
      //bonus decay didn't kick in so give full bonus
      return pv * 2;
    }

    int step = floor(sec_passed / lbconf.compound_step_sec);  //calculate in which compounding/decay step we are.
    double r = lbconf.compound_decay_pct;
    uint64_t bonus = (uint64_t)(pv * pow(1 - r, step));
    uint64_t score = bonus + pv;
    return score;
  }

  void modify_round(uint64_t& score, asset& donation, donations::rounds_table& idx, donations::rounds_table::const_iterator& itr, bool& has_donated) {
    idx.modify(itr, eosio::same_payer, [&](auto& n) {
      n.total_donated += donation;
      n.total_score += score;
      n.donators = has_donated ? n.donators : n.donators + 1;
    });
  }

  void emplace_round(uint64_t& score, asset& donation, name& donator, donations::rounds_table& idx) {
    if(lbconf.minimum_donation > donation) {
      eosio::print("Not eligible for leaderboard. First donation must be minimum " + donation.to_string());
    } else {
      idx.emplace(_self, [&](donations::rounds& n) {
        n.id = round_id;
        n.total_donated = donation;
        n.total_score = score;
        n.donators = 1;
        n.start = get_round_start_time();
        n.rewarded = false;
      });
    }
  }

  void modify_leaderboard(uint64_t& score, asset& donation, donations::leaderboard_table& idx, donations::leaderboard_table::const_iterator& itr) {
    idx.modify(itr, eosio::same_payer, [&](auto& n) {
      n.times++;
      n.score += score;
      n.donated += donation;
      n.last_donation = now;
    });
  }

  void emplace_leaderboard(uint64_t& score, asset& donation, name& donator, donations::leaderboard_table& idx) {
    if(lbconf.minimum_donation > donation) {
      eosio::print("Not eligible for leaderboard. First donation must be minimum " + donation.to_string());
    } else {
      idx.emplace(_self, [&](donations::leaderboard& n) {
        n.donator = donator;
        n.times = 1;
        n.score = score;
        n.donated = donation;
        n.last_donation = now;
      });
    }
  }
  eosio::time_point_sec get_round_start_time() {
    return eosio::time_point_sec(lbconf.start_time.sec_since_epoch() + ((round_id - 1) * lbconf.round_length_sec));
  }
};
