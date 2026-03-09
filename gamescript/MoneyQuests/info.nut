/*
    MoneyQuests — info.nut  v1.0
    Optional bonus delivery quests for OpenTTD Archipelago
    Author: Marco Jonas Christiansen

    Activate via Game Scripts in the new game settings.
    No connection to Archipelago — pure money rewards only.
*/

class MoneyQuestsInfo extends GSInfo {
    function GetAuthor()      { return "Marco Jonas Christiansen"; }
    function GetName()        { return "Money Quests"; }
    function GetDescription() { return "Optional bonus delivery quests. Deliver cargo to a town before the deadline and earn extra cash. Works alongside Archipelago — no interference."; }
    function GetVersion()     { return 1; }
    function GetDate()        { return "2026-03-09"; }
    function GetShortName()   { return "MQGS"; }
    function GetAPIVersion()  { return "14"; }
    function CreateInstance() { return "MoneyQuests"; }
    function UseAsRandomGS()  { return false; }

    function GetSettings()
    {
        AddSetting({
            name = "max_active_quests",
            description = "Maximum active quests per company",
            default_value = 3, min_value = 1, max_value = 6,
            flags = CONFIG_INGAME
        });
        AddSetting({
            name = "deadline_years",
            description = "Years to complete each quest",
            default_value = 2, min_value = 1, max_value = 5,
            flags = CONFIG_INGAME
        });
        AddSetting({
            name = "reward_multiplier",
            description = "Reward generosity",
            default_value = 2, min_value = 0, max_value = 4,
            flags = CONFIG_INGAME
        });
        AddLabels("reward_multiplier", {
            _0 = "Modest",
            _1 = "Normal",
            _2 = "Generous",
            _3 = "Rich",
            _4 = "Jackpot"
        });
        AddSetting({
            name = "difficulty",
            description = "Delivery amount difficulty",
            default_value = 1, min_value = 0, max_value = 3,
            flags = CONFIG_INGAME
        });
        AddLabels("difficulty", {
            _0 = "Easy",
            _1 = "Normal",
            _2 = "Hard",
            _3 = "Extreme"
        });
    }
}

RegisterGS(MoneyQuestsInfo());
