
//temporary defines... they can be in a table uint32_t
uint32_t  TEMPLATE_ID_GOLD = 111111;
uint32_t  TEMPLATE_ID_SILVER = 22222;
uint32_t  TEMPLATE_ID_BRONZE = 33333;


void donations::on_nft_transfer(name from, name to, vector <uint64_t> asset_ids, string memo){
    vector <uint32_t> allowed_templates{TEMPLATE_ID_BRONZE, TEMPLATE_ID_SILVER, TEMPLATE_ID_GOLD};
    //this map holds the allowed template ids as well as how many are transferred.
    //must moved in a separate fn
    
    map<uint32_t,int> template_counts;

    //loop through all asset_ids and determine what template_id they have and update it's counter;
    atomicassets::assets_t receiver_assets = atomicassets::get_assets(to);
    for (std::vector<int>::size_type i = 0; i != asset_ids.size(); i++){
        auto asset_itr = receiver_assets.find(asset_ids[i]);
        bool found = false;
       
        for (std::vector<int>::size_type i = 0; i != allowed_templates.size(); i++){
            if(allowed_templates[i] == asset_itr->template_id){
                if(template_counts.find(asset_itr->template_id) != template_counts.end() ){ 
                    template_counts[asset_itr->template_id] += 1;
                }
                else{
                    template_counts[asset_itr->template_id] = 1;
                }
                found = true;
                break;
            }
        }

        check(found, "Asset not in the list of allowed templates (gold, silver, bronze)");
    }
    /////////////////////////////


    if(to == get_self() ){
        //if to == self than the contract expects a set of NFTs to be burned and a new NFT to be minted
        //validate set  (ie minimum amount of 10 silver)
        uint32_t set_type =  get_set_template_id(template_counts);
        //burn(asset_ids)
        if(set_type == TEMPLATE_ID_BRONZE){
            //mint silver
        }
        if(set_type == TEMPLATE_ID_SILVER){
            //mint gold
        }
    }
    else{
        //nft transfered to new owner
        //does minting also triggers a transfer? this is not handled-> must check from account it that case*

        //the account table needs to be updated with the counted values
        // donations::accounts_table _accounts(get_self(), get_self().value);

        // add_nfts(to, template_counts,_accounts);
        // sub_nfts(from, template_counts,_accounts);

    }
}


//table references are passed as argument to prevent initializing the table more than once
//  void donations::add_nfts(name& account, map<uint32_t,int> nft_deltas,  donations::accounts_table& idx) {
//         auto itr = idx.find(account.value);
//         if(itr == idx.end() ){
//             //new receiver
//             idx.emplace(get_self(), [&](donations::accounts& n) {
//                 n.account = account;
//                 // n.bronze_unclaimed = nft_deltas.contains(name("unclaimed")) ? nft_deltas[name("unclaimed")] : 0;
//                 n.bronze_claimed = nft_deltas.contains(TEMPLATE_ID_BRONZE) ? nft_deltas[TEMPLATE_ID_BRONZE] : 0;
//                 n.silver_claimed = nft_deltas.contains(TEMPLATE_ID_SILVER) ? nft_deltas[TEMPLATE_ID_SILVER] : 0;
//                 n.gold_claimed = nft_deltas.contains(TEMPLATE_ID_GOLD) ? nft_deltas[TEMPLATE_ID_GOLD] : 0;
//             });
//         }
//         else{
//             idx.modify(itr, eosio::same_payer, [&](auto& n) {
//                 // n.bronze_unclaimed += nft_deltas.contains(name("unclaimed")) ? nft_deltas[name("unclaimed")] : 0;
//                 n.bronze_claimed += nft_deltas.contains(TEMPLATE_ID_BRONZE) ? nft_deltas[TEMPLATE_ID_BRONZE] : 0;
//                 n.silver_claimed += nft_deltas.contains(TEMPLATE_ID_SILVER) ? nft_deltas[TEMPLATE_ID_SILVER] : 0;
//                 n.gold_claimed += nft_deltas.contains(TEMPLATE_ID_GOLD) ? nft_deltas[TEMPLATE_ID_GOLD] : 0;
//             });
//         }    
//   }


// void donations::sub_nfts(name& account, map<uint32_t,int> nft_deltas,  donations::accounts_table& idx) {
//         auto itr = idx.require_find(account.value, "It should not be possible to transfer NFTs if not yet registered in the accounts table.");//*
//         // check(itr->bronze_unclaimed >= nft_deltas[name("unclaimed")], "not enough unclaimed bronze nfts");//
//         check(itr->gold_claimed >= nft_deltas[TEMPLATE_ID_GOLD], "not enough gold nfts");//
//         check(itr->silver_claimed >= nft_deltas[TEMPLATE_ID_SILVER], "not enough silver nfts");//
//         check(itr->bronze_claimed >= nft_deltas[TEMPLATE_ID_BRONZE], "not enough bronze nfts");//


//         idx.modify(itr, eosio::same_payer, [&](auto& n) {
//             // n.bronze_unclaimed -= nft_deltas.contains(name("unclaimed")) ? nft_deltas[name("unclaimed")] : 0;
//             n.bronze_claimed -= nft_deltas.contains(TEMPLATE_ID_BRONZE) ? nft_deltas[TEMPLATE_ID_BRONZE] : 0;
//             n.silver_claimed -= nft_deltas.contains(TEMPLATE_ID_SILVER) ? nft_deltas[TEMPLATE_ID_SILVER] : 0;
//             n.gold_claimed -= nft_deltas.contains(TEMPLATE_ID_GOLD) ? nft_deltas[TEMPLATE_ID_GOLD] : 0;
//         });
// }

uint32_t donations::get_set_template_id(map<uint32_t,int> nft_deltas){
        check(nft_deltas.size() == 1, "all nfts must be of the same type");
        uint32_t template_id = nft_deltas.begin()->first; // key=template_id
        int quantity = nft_deltas.begin()->second; //how many nfts

        if(template_id == TEMPLATE_ID_BRONZE){
            check(quantity == 20, "You need to transfer exactly 20 bronze nfts to mint a silver one" );
        }
        if(template_id == TEMPLATE_ID_SILVER){
            check(quantity == 10, "You need to transfer exactly 10 silver nfts to mint a golden one" );
        }
        if(template_id == TEMPLATE_ID_GOLD){
            check(false, "Golden NFTs can't be burned at this time" );
        }

        //this will only return if the set is valid
        return template_id;
}

