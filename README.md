<hr />
# Raicoin(RAI): a block-lattice coin with NODE REWARD and many NEW FEATURES
<hr />

Raicoin will be developed based on the ideas of NANO(formerly Raiblocks) but with many different and new features.


1. **Node Reward**: all nodes with election weight can earn reward.
2. **Credit Mechanism**: NANO uses POW to anti transaction flooding attack, this approach will be replaced by the new credit system.
   * Accounts destroy little amount coin to get credits from the network, than allowed to do (20 x credits) transactions every day(In a word, buy once for life).
   * Any account must have at least 1 credit when it is created.
   * The cost of credit will gradually decrease.
   * Transactions from accounts with high remaining percentage of allowed transactions will be processed first when the network is busy.
   
3. **Achievable and Sustainable high performance**.
    * Two types of nodes are designed: light node and full node. light nodes can not only do pruning but also unpruning when needed.
    * light nodes are designed to run on ramdisk/NVMe/SSD as election nodes, and full nodes run on HDD as backup nodes.
    * Rough IO performance on different device: 10k+ txs/s (ramdisk), 2k+ txs/s (NVMe), 1k txs/s(SSD), hundreds txs/s(HDD).
    
4. **Bandwidth Saving**: election on block will be changed to election on fork, save up to 90% bandwidth cost.

5. **Custom Data Field**: the protocol will add a custom variable-length data field. This field can be used to take extra infomation(e.g. transaction notes, order number, user id).

6. **Privacy**: mixer addon can be developed based on the custom data field. Third party will be happy to develop and run a mixer to get more users and more node reward. An offical implementation may also be developed if necessary.

7. **Distribution**
    * All coins are generated by node reward.
    * New generated coins of each year:
        1st year: 40M,
        2nd year: 30M,
        3rd year: 20M,
        4th year: 10M,
        from 5th year: about 5% coins
        
8. **Developer Fund**: no coins will be reserved as developer fund, the project is planned to be operated by donation and volunteers.
        
9.  **Airdrop**
    * Airdrop is randomly distributed based on the total online time of the node, that is, the higher online time, the higher probability of getting airdrop.
    * Airdrop will last 4 years, the approximate amounts by time:
1st quarter of 1st year: 7M,
        2nd quarter of 1st year: 4M,
        3rd quarter of 1st year: 3M,
        4th quarter of 1st year: 2M,
        2nd year: 5M,
        3rd year: 2M,
        4th year: 1M.
    * In first 2 quarters, only invited nodes can get airdrop. And after then all nodes can get airdrop.
    * How to get invitations?
        * To be active github contributors for the project.
        * By donation, the minimum amounts of BTC/NANO for an invitation are as follows(UTC time):
            Sep. 2019: 0.001BTC/10NANO
            Oct. 2019: 0.005BTC/50NANO
            Nov. 2019: 0.01BTC/100NANO
            Dec. 2019: 0.02BTC/200NANO
            Jan. 2020: 0.03BTC/300NANO
            Feb. 2020: 0.04BTC/400NANO
            Mar. 2020: 0.05BTC/500NANO
            Apr. 2020: 0.06BTC/600NANO
            May  2020: 0.07BTC/700NANO
            June 2020: 0.08BTC/800NANO
            July 2020: 0.09BTC/900NANO
            Aug. 2020: 0.1BTC/1000NANO
            After then: 0.2BTC/2000NANO
            
**Roadmap**
  * Beta release: March 31, 2020.
  * Live release: September 30, 2020.
    
**Announcement**：https://bitcointalk.org/index.php?topic=5188094.0
**github repositories**: https://github.com/RaicoinCommunity

**Donation Address**
BTC: **1eZdfnsxFNqzcJioPRAr2hmRPMZ9GmPjL**
NANO: **nano_1t8yfzbmyz7eqeowgx9ij4h7mftwo4qgke3empdpge8o9wug1y8urh6giuhx**
(Please send from an address you control, not an exchange address. To get invitation, you will need to sign your raicoin node id with the BTC/NANO address.)
