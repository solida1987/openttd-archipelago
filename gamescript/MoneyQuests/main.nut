/*
    MoneyQuests — main.nut  v1.0
    Bonus delivery quests for OpenTTD Archipelago

    Each company gets up to N active quests.
    Each quest: "Deliver X tons of [Cargo] to [Town] before [Date]"
    Reward: cash paid directly to the company.
    Failure: quest expires silently, new one generated.

    Tracks delivery via GSCargoMonitor (per-town, per-cargo, per-company).
    Story Page shows all active quests with live progress.
*/

class MoneyQuests extends GSController
{
    /* ── Per-company quest list ─────────────────────────────────── */
    quests       = null;   // co -> [ {quest}, ... ]
    story_pages  = null;   // co -> story_page_id
    btn_map      = null;   // co -> { btn_eid -> quest_index }  (unused, future)

    /* ── Cargo pool (vanilla) ───────────────────────────────────── */
    CARGO_LABELS = ["PASS","MAIL","COAL","OIL_","LVST","GOOD","GRAI","WOOD",
                    "IORE","STEL","VALU","WHEA","PAPR","GOLD","RUBR","FRUT",
                    "MAIZ","CORE","WATR","DIAM","SUGR","TOYS","BATT","SWET",
                    "TOFF","COLA","CTCD","BUBL","PLST","FZDR"];

    /* Amount bases per difficulty (tons) */
    AMOUNT_BASE  = [800, 2500, 8000, 25000];

    /* Reward per unit (£) scales with multiplier setting */
    REWARD_PER_UNIT = [40, 80, 160, 320, 640];

    function constructor()
    {
        this.quests      = {};
        this.story_pages = {};
        this.btn_map     = {};
    }

    /* ════════════════════════════════════════════════════════════════
       MAIN LOOP
       ════════════════════════════════════════════════════════════════ */
    function Start()
    {
        GSController.Sleep(100);  // wait for map to settle

        local companies = this._GetCompanies();
        foreach (co in companies) this._EnsureCompany(co);

        local tick = 0;
        while (true) {
            GSController.Sleep(74);  // ~1 game day
            tick++;

            /* Monthly logic (74 ticks ≈ 1 day; ~2220 ticks ≈ 1 month) */
            if (tick % 2220 == 0) {
                local cos = this._GetCompanies();
                foreach (co in cos) {
                    this._EnsureCompany(co);
                    this._UpdateDeliveries(co);
                    this._CheckDeadlines(co);
                    this._TryGenerateQuests(co);
                    this._RefreshPage(co);
                }
            }
        }
    }

    /* ════════════════════════════════════════════════════════════════
       SAVE / LOAD
       ════════════════════════════════════════════════════════════════ */
    function Save()
    {
        return { quests = this.quests };
    }

    function Load(version, data)
    {
        if ("quests" in data) this.quests = data.quests;
        /* Re-create story pages after load */
        local companies = this._GetCompanies();
        foreach (co in companies) {
            this._EnsureCompany(co);
            this._RefreshPage(co);
        }
    }

    /* ════════════════════════════════════════════════════════════════
       COMPANY SETUP
       ════════════════════════════════════════════════════════════════ */
    function _EnsureCompany(co)
    {
        if (!this.quests.rawin(co))      this.quests[co]      = [];
        if (!this.story_pages.rawin(co)) this._CreatePage(co);
    }

    function _GetCompanies()
    {
        local list = GSCompanyList();
        local result = [];
        for (local co = list.Begin(); !list.IsEnd(); co = list.Next()) {
            result.push(co);
        }
        return result;
    }

    /* ════════════════════════════════════════════════════════════════
       DELIVERY TRACKING
       ════════════════════════════════════════════════════════════════ */
    function _UpdateDeliveries(co)
    {
        if (!this.quests.rawin(co)) return;
        foreach (q in this.quests[co]) {
            if (!q.active) continue;
            /* GSCargoMonitor accumulates since last call with keep_monitoring=true */
            local delivered = GSCargoMonitor.GetTownDeliveryAmount(co, q.cargo_id, q.town_id, true);
            if (delivered > 0) q.delivered += delivered;

            /* Quest complete! */
            if (q.delivered >= q.amount) {
                q.active    = false;
                q.completed = true;
                local reward = q.amount * this.GetSetting("reward_multiplier") <= 0 ?
                               q.reward_base :
                               q.reward_base;  // reward_base already scaled
                GSCompany.ChangeBankBalance(co, reward, GSCompany.EXPENSES_OTHER, GSMap.TILE_WRAP);
                GSNews.Create(GSNews.NT_GENERAL, co,
                    "Money Quest complete! Delivered " + q.cargo_name +
                    " to " + q.town_name + " — Earned £" + reward + "!");
            }
        }
        /* Remove completed/expired */
        local active = [];
        foreach (q in this.quests[co]) {
            if (q.active) active.push(q);
        }
        this.quests[co] = active;
    }

    /* ════════════════════════════════════════════════════════════════
       DEADLINE CHECK
       ════════════════════════════════════════════════════════════════ */
    function _CheckDeadlines(co)
    {
        if (!this.quests.rawin(co)) return;
        local now_y = GSDate.GetYear(GSDate.GetCurrentDate());
        local now_m = GSDate.GetMonth(GSDate.GetCurrentDate());

        foreach (q in this.quests[co]) {
            if (!q.active) continue;
            if (now_y > q.deadline_year || (now_y == q.deadline_year && now_m > q.deadline_month)) {
                q.active  = false;
                q.expired = true;
                GSNews.Create(GSNews.NT_GENERAL, co,
                    "Money Quest expired: " + q.cargo_name + " to " + q.town_name + ".");
            }
        }
        local active = [];
        foreach (q in this.quests[co]) {
            if (q.active) active.push(q);
        }
        this.quests[co] = active;
    }

    /* ════════════════════════════════════════════════════════════════
       QUEST GENERATION
       ════════════════════════════════════════════════════════════════ */
    function _TryGenerateQuests(co)
    {
        if (!this.quests.rawin(co)) return;
        local max_q = this.GetSetting("max_active_quests");
        while (this.quests[co].len() < max_q) {
            local q = this._GenerateQuest(co);
            if (q == null) break;
            this.quests[co].push(q);
        }
    }

    function _GenerateQuest(co)
    {
        /* Pick a random town with at least one station */
        local town_list = GSTownList();
        local towns = [];
        for (local t = town_list.Begin(); !town_list.IsEnd(); t = town_list.Next()) {
            towns.push(t);
        }
        if (towns.len() == 0) return null;

        /* Shuffle towns — pick first viable one */
        local town_id   = null;
        local town_name = null;
        local attempts  = 0;
        while (attempts < 20) {
            local idx = GSBase.RandRange(towns.len());
            local t   = towns[idx];
            town_id   = t;
            town_name = GSTown.GetName(t);
            break;
            attempts++;
        }
        if (town_id == null) return null;

        /* Pick a cargo available on the current landscape */
        local cargo_id   = null;
        local cargo_name = null;
        local cargo_attempts = 0;
        while (cargo_id == null && cargo_attempts < 30) {
            local lbl = this.CARGO_LABELS[GSBase.RandRange(this.CARGO_LABELS.len())];
            local cid = GSCargo.GetCargoIDByLabel(GSCargo.StringToCargoLabel(lbl));
            if (cid != null && GSCargo.IsValidCargo(cid)) {
                cargo_id   = cid;
                cargo_name = GSCargo.GetName(cid);
            }
            cargo_attempts++;
        }
        if (cargo_id == null) return null;

        /* Amount based on difficulty + game year (scales slightly over time) */
        local diff       = this.GetSetting("difficulty");
        local base       = this.AMOUNT_BASE[diff];
        local year_bonus = (GSDate.GetYear(GSDate.GetCurrentDate()) - 1950) / 10;
        if (year_bonus < 0) year_bonus = 0;
        local amount = base + (base * year_bonus / 4);

        /* Reward */
        local mult        = this.GetSetting("reward_multiplier");
        local reward_base = amount * this.REWARD_PER_UNIT[mult];

        /* Deadline */
        local now_y      = GSDate.GetYear(GSDate.GetCurrentDate());
        local now_m      = GSDate.GetMonth(GSDate.GetCurrentDate());
        local years      = this.GetSetting("deadline_years");
        local dl_month   = now_m;
        local dl_year    = now_y + years;

        /* Start monitoring */
        GSCargoMonitor.GetTownDeliveryAmount(co, cargo_id, town_id, true);

        return {
            active        = true,
            completed     = false,
            expired       = false,
            cargo_id      = cargo_id,
            cargo_name    = cargo_name,
            town_id       = town_id,
            town_name     = town_name,
            amount        = amount,
            delivered     = 0,
            deadline_year = dl_year,
            deadline_month = dl_month,
            reward_base   = reward_base
        };
    }

    /* ════════════════════════════════════════════════════════════════
       STORY PAGE
       ════════════════════════════════════════════════════════════════ */
    function _CreatePage(co)
    {
        local page = GSStoryPage.New(co, "Money Quests");
        if (!GSStoryPage.IsValidStoryPage(page)) return;
        this.story_pages[co] <- page;
        this._RefreshPage(co);
    }

    function _RefreshPage(co)
    {
        if (!this.story_pages.rawin(co)) return;
        local page = this.story_pages[co];
        if (!GSStoryPage.IsValidStoryPage(page)) {
            this._CreatePage(co);
            return;
        }

        /* Remove all existing elements */
        local el_list = GSStoryPageElementList(page);
        for (local el = el_list.Begin(); !el_list.IsEnd(); el = el_list.Next()) {
            GSStoryPageElement.Remove(el);
        }

        /* Header */
        GSStoryPageElement.NewText(page, 0, "── Money Quests ──────────────────────────────");
        GSStoryPageElement.NewText(page, 1, "Deliver cargo to earn bonus cash. No Archipelago");
        GSStoryPageElement.NewText(page, 2, "checks involved — pure money rewards.");
        GSStoryPageElement.NewText(page, 3, " ");

        if (!this.quests.rawin(co) || this.quests[co].len() == 0) {
            GSStoryPageElement.NewText(page, 4, "No active quests. Check back next month.");
            return;
        }

        local y = 4;
        local i = 1;
        foreach (q in this.quests[co]) {
            if (!q.active) continue;
            local pct = (q.delivered * 100) / q.amount;
            if (pct > 100) pct = 100;
            local bar = this._ProgressBar(pct);
            GSStoryPageElement.NewText(page, y++,
                "Quest " + i + ": Deliver " + q.amount + " " + q.cargo_name);
            GSStoryPageElement.NewText(page, y++,
                "  → Destination: " + q.town_name);
            GSStoryPageElement.NewText(page, y++,
                "  → Deadline: " + q.deadline_year + "/" + q.deadline_month);
            GSStoryPageElement.NewText(page, y++,
                "  → Reward: £" + q.reward_base);
            GSStoryPageElement.NewText(page, y++,
                "  → Progress: " + q.delivered + "/" + q.amount + "  " + bar);
            GSStoryPageElement.NewText(page, y++, " ");
            i++;
        }
    }

    function _ProgressBar(pct)
    {
        local filled = pct / 10;
        local bar = "[";
        for (local i = 0; i < 10; i++) {
            bar += (i < filled) ? "#" : "-";
        }
        bar += "] " + pct + "%";
        return bar;
    }
}
