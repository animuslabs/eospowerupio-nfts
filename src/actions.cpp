#include <atomicdata.hpp>
#include <donations.hpp>
#include <leaderboard.hpp>

ACTION donations::setconfig(const config& cfg) {
  require_auth(get_self());
  //validate config
  check(cfg.round_length_sec > 0, "round_length_sec must be greater than 0");
  check(cfg.compound_step_sec > 0, "compound_step_sec must be greater than 0");
  check(cfg.round_length_sec > cfg.compound_step_sec, "round_length_sec must be greater then compound_step_sec");
  // check(cfg.start_time > NOW , "start_time must be in the future");
  //todo, review config checks.

  config_table _config(get_self(), get_self().value);
  _config.set(cfg, get_self());
}

ACTION donations::clrconfig() {
  require_auth(get_self());
  config_table _config(get_self(), get_self().value);
  check(_config.exists(), "Config table already removed");
  _config.remove();
}

ACTION donations::clrround(uint64_t& round_id) {
  require_auth(get_self());
  rounds_table _rounds(get_self(), get_self().value);
  auto r_itr = _rounds.find(round_id);
  check(r_itr != _rounds.end(), "Round with this id not found.");

  leaderboard_table _leaderboard(get_self(), round_id);
  check(_leaderboard.begin() == _leaderboard.end(), "Clear the leaderboard with round_id/scope " + to_string(round_id) + "first.");

  _rounds.erase(r_itr);
};

ACTION donations::rewardround(uint64_t& round_id) {
  Leaderboard lb(get_self());
  check(round_id != lb.round_id, "can't reward current round, wait until next round starts");
  rounds_table _rounds(get_self(), get_self().value);
  auto r_itr = _rounds.require_find(round_id, "round doesn't exist");
  // check(!r_itr->rewarded, "rewards already generated for this round"); // temp for debugging
  const auto reward_data = lb.generate_round_rewards(round_id, *r_itr);
  action(active_auth, get_self(), name("rewardlog"), make_tuple(*r_itr, reward_data)).send();
  _rounds.modify(r_itr, get_self(), [&](rounds& row) {
    row.rewarded = true;
  });
};

ACTION donations::rewardlog(rounds& round_data, vector<rewards_data>& rewards_data) {
  require_auth(get_self());
};

ACTION donations::rmaccount(name& donator) {
  check(has_auth(get_self()) || has_auth(donator), "not authorized");
  accounts_table _accounts(get_self(), get_self().value);
  const auto donator_itr = _accounts.require_find(donator.value, "donator doesn't have an account");
  _accounts.erase(donator_itr);
};

ACTION donations::claim(name& donator) {
  // check(has_auth(get_self()) || has_auth(donator), "not authorized");
  donations::accounts_table _accounts(_self, _self.value);
  config_table _config(get_self(), get_self().value);
  const auto config = _config.get();
  const auto donator_itr = _accounts.require_find(donator.value, "donator doesn't have an account");
  const auto blank_data = atomicdata::ATTRIBUTE_MAP {};
  const vector<asset> tokens_to_back;
  auto claimer = *donator_itr;
  check(claimer.bronze_unclaimed > 0, "no nfts to claim");
  const auto bronze_data = make_tuple(get_self(), config.nft.collection_name, config.nft.schema_name, config.nft.bronze_template_id, get_self(), blank_data, blank_data, tokens_to_back);
  const action mint_bronze = action(active_auth, name("atomicassets"), name("mintasset"), bronze_data);
  auto bronze_minted = 0;
  while(bronze_minted != claimer.bronze_unclaimed) {
    mint_bronze.send();
    bronze_minted++;
  }
  claimer.bronze_claimed += bronze_minted;
  claimer.bronze_unclaimed = 0;

  const auto total_mint_silver = claimer.bronze_claimed / config.nft.bronze_to_silver;
  if(total_mint_silver > claimer.silver_claimed) {
    auto mint_silver = total_mint_silver - claimer.silver_claimed;
    const auto silver_data = make_tuple(get_self(), config.nft.collection_name, config.nft.schema_name, config.nft.silver_template_id, get_self(), blank_data, blank_data, tokens_to_back);
    const action mint_silver_action = action(active_auth, name("atomicassets"), name("mintasset"), silver_data);
    auto silver_minted = 0;
    while(silver_minted != mint_silver) {
      mint_silver_action.send();
      silver_minted++;
    }
    claimer.silver_claimed += silver_minted;
  }

  const auto total_mint_gold = claimer.silver_claimed / config.nft.silver_to_gold;
  if(total_mint_gold > claimer.gold_claimed) {
    auto mint_gold = total_mint_gold - claimer.gold_claimed;
    const auto gold_data = make_tuple(get_self(), config.nft.collection_name, config.nft.schema_name, config.nft.gold_template_id, get_self(), blank_data, blank_data, tokens_to_back);
    const action mint_gold_action = action(active_auth, name("atomicassets"), name("mintasset"), gold_data);
    auto gold_minted = 0;
    while(gold_minted != mint_gold) {
      mint_gold_action.send();
      gold_minted++;
    }
    claimer.gold_claimed += gold_minted;
  }

  _accounts.modify(donator_itr, _self, [&](donations::accounts& row) {
    row = claimer;
  });
};
