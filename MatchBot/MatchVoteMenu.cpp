#include "precompiled.h"

CMatchVoteMenu gMatchVoteMenu;

// Server Activate
void CMatchVoteMenu::ServerActivate()
{
	// Clear Votes
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		// Clear all votes
		this->m_Votes[i].Reset();
	}

	// Clear Map List
	this->m_MapList.clear();

	// Get Map List and skip current map
	auto Maps = gMatchUtil.GetMapList(false);

	// Loop Map List
	for (auto const & row : Maps)
	{
		// Add to map pool
		this->m_MapList[row.first] = row.second;
	}
}

// Player Disconnect
void CMatchVoteMenu::PlayerDisconnect(edict_t* pEdict)
{
	// Get Player entity Index
	auto EntityIndex = ENTINDEX(pEdict);

	// Reset Player Votes
	this->m_Votes[EntityIndex].Reset();

	// Loop Clients to reset Vote Kick
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		// Reset Vote Kick Count from other players to player
		this->m_Votes[i].VoteKick[EntityIndex] = false;
	}
}

// Main Vote Menu
bool CMatchVoteMenu::Menu(CBasePlayer* Player)
{
	if (Player->m_iTeam == TERRORIST || Player->m_iTeam == CT)
	{
		if (gMatchBot.GetState() != STATE_START)
		{
			auto EntityIndex = Player->entindex();

			gMatchMenu[EntityIndex].Create(_T("Player Vote Menu:"), true, (void*)this->MenuHandle);

			gMatchMenu[EntityIndex].AddItem(0, _T("Vote for Kick"));

			gMatchMenu[EntityIndex].AddItem(1, _T("Vote for Map"));

			gMatchMenu[EntityIndex].AddItem(2, _T("Vote for Tactical Timeout"));

			gMatchMenu[EntityIndex].AddItem(3, _T("Vote for Restart Match"));

			gMatchMenu[EntityIndex].Show(EntityIndex);

			return true;
		}
	}

	gMatchUtil.SayText(Player->edict(), PRINT_TEAM_DEFAULT, _T("Unable to use this command now."));

	return false;
}

// Main Vote Menu Handle
void CMatchVoteMenu::MenuHandle(int EntityIndex, P_MENU_ITEM Item)
{
	auto Player = UTIL_PlayerByIndexSafe(EntityIndex);

	if (Player)
	{
		switch(Item.Info)
		{
			case 0: // Vote Kick
			{
				gMatchVoteMenu.VoteKick(Player);
				break;
			}
			case 1: // Vote Map
			{
				gMatchVoteMenu.VoteMap(Player);
				break;
			}
			case 2: // Vote Pause
			{
				gMatchVoteMenu.VotePause(Player);
				break;
			}
			case 3: // Vote Restart
			{
				gMatchVoteMenu.VoteRestart(Player);
				break;
			}
		}
	}
}

// Vote Kick Menu
bool CMatchVoteMenu::VoteKick(CBasePlayer* Player)
{
	// If player is in game
	if (Player->m_iTeam == TERRORIST || Player->m_iTeam == CT)
	{
		// If vote state is not running
		if (gMatchBot.GetState() != STATE_START)
		{
			// Create menu
			gMatchMenu[Player->entindex()].Create(_T("Vote Kick:"), true, (void*)this->VoteKickHandle);

			// Get players from player team
			auto Players = gMatchUtil.GetPlayers(Player->m_iTeam, true);

			// If we have the needed players to vote kick
			if (Players.size() >= VOTE_MENU_KICK_PLAYERS_MIN)
			{
				// Get the total of votes needed to kick (Subtract the player itself vote)
				auto VotesNeed = (Players.size() - 1);

				// Loop players
				for (auto Target : Players)
				{
					// If is not equal the player
					if (Target->entindex() != Player->entindex())
					{
						// If is not admin immunity
						if (!gMatchAdmin.Access(Target->entindex(), ADMIN_IMMUNITY))
						{
							// Get percentage of vote count
							auto VoteCount = 0;

							// Loop client votes
							for (int i = 1; i <= gpGlobals->maxClients; ++i)
							{
								// Check if voted
								if (this->m_Votes[i].VoteKick[Target->entindex()])
								{
									VoteCount++;
								}
							}

							// Add to menu
							gMatchMenu[Player->entindex()].AddItem(Target->entindex(), gMatchUtil.FormatString("%s \\y\\R%2.0f%%", STRING(Target->edict()->v.netname), (float)(VoteCount * 100 / VotesNeed)), this->m_Votes[Player->entindex()].VoteKick[Target->entindex()]);
						}
					}
				}
				
				// Show Menu
				gMatchMenu[Player->entindex()].Show(Player->entindex());
			}
			else
			{
				// Message
				gMatchUtil.SayText(Player->edict(), PRINT_TEAM_DEFAULT, _T("We need \3%d\1 players in \3%s\1 team to Vote Kick."), VOTE_MENU_KICK_PLAYERS_MIN, gMatchBot.GetTeam(Player->m_iTeam, false));
			}

			return true;
		}
	}
	
	// Failed command
	gMatchUtil.SayText(Player->edict(), PRINT_TEAM_DEFAULT, _T("Unable to use this command now."));

	return false;
}

void CMatchVoteMenu::VoteKickHandle(int EntityIndex, P_MENU_ITEM Item)
{
	auto Player = UTIL_PlayerByIndexSafe(EntityIndex);

	if (Player)
	{
		auto Target = UTIL_PlayerByIndexSafe(Item.Info);

		if (Target)
		{
			auto PlayerIndex = Player->entindex();
			auto TargetIndex = Target->entindex();

			if (!Item.Disabled)
			{
				gMatchVoteMenu.m_Votes[PlayerIndex].VoteKick[TargetIndex] = true;

				// Get percentage of vote count
				auto VoteCount = 0;

				// Loop clients
				for (int i = 1; i <= gpGlobals->maxClients; ++i)
				{
					// Check if voted
					if (gMatchVoteMenu.m_Votes[i].VoteKick[Target->entindex()])
					{
						VoteCount++;
					}
				}

				// Get votes needed
				auto VotesNeed = (gMatchUtil.GetCount(Player->m_iTeam) - 1);

				// Vote Map Progress to change map
				auto VoteProgress = (float)((VoteCount * 100) / VotesNeed);

				// If reached count
				if (VoteProgress < 100.0f)
				{
					// Send vote kick messages
					gMatchUtil.SayText(nullptr, PlayerIndex, _T("\3%s\1 voted to kick \3%s\1: %2.0f%% of votes to kick."), STRING(Player->edict()->v.netname), STRING(Target->edict()->v.netname), VoteProgress);
					gMatchUtil.SayText(nullptr, PlayerIndex, _T("Say \3.vote\1 to open vote kick."));
				}
				else
				{
					// Kick Player
					gMatchUtil.ServerCommand("kick #%d %s", GETPLAYERUSERID(Target->edict()), _T("Kicked by Vote Kick."));

					// Send Mesasge
					gMatchUtil.SayText(nullptr, TargetIndex, _T("\3%s\1 Kicked: \4%d\1 votes reached."), STRING(Target->edict()->v.netname), VotesNeed);
				}
			}
			else
			{
				gMatchUtil.SayText(Player->edict(), TargetIndex, _T("Already voted to kick: \3%s\1..."), STRING(Target->edict()->v.netname));
			}
		}
	}
}

// Vote Map Menu
bool CMatchVoteMenu::VoteMap(CBasePlayer* Player)
{
	// If player is in game
	if (Player->m_iTeam == TERRORIST || Player->m_iTeam == CT)
	{
		// If vote state is not running
		if (gMatchBot.GetState() != STATE_START)
		{
			// Create menu
			gMatchMenu[Player->entindex()].Create(_T("Vote Map:"), true, (void*)this->VoteMapHandle);

			// Get Players
			auto Players = gMatchUtil.GetPlayers(true, true);

			// Needed votes
			auto VotesNeed = (Players.size() * VOTE_MENU_PERCENTAGE);

			// Loop Map List
			for (auto const& row : this->m_MapList)
			{
				// Get Vote Count
				auto VoteCount = 0;

				// Loop in game players
				for (const auto& Temp : Players)
				{
					// If has voted on that map
					if (this->m_Votes[Temp->entindex()].VoteMap[row.first])
					{
						// Count vote
						VoteCount++;
					}
				}

				// Add Menu Item
				gMatchMenu[Player->entindex()].AddItem(row.first, gMatchUtil.FormatString("%s \\y\\R%2.0f%%", row.second.c_str(), (float)((VoteCount * 100) / VotesNeed)), this->m_Votes[Player->entindex()].VoteMap[row.first]);
			}

			// Show Menu to player
			gMatchMenu[Player->entindex()].Show(Player->entindex());

			return true;
		}
	}

	// Failed command
	gMatchUtil.SayText(Player->edict(), PRINT_TEAM_DEFAULT, _T("Unable to use this command now."));

	return false;
}

// Vote Map Menu Handle
void CMatchVoteMenu::VoteMapHandle(int EntityIndex, P_MENU_ITEM Item)
{
	auto Player = UTIL_PlayerByIndexSafe(EntityIndex);

	if (Player)
	{
		// Get Voted Map
		auto MapName = gMatchVoteMenu.m_MapList[Item.Info];

		// If can vote in that map
		if (!Item.Disabled)
		{
			// Add Vote
			gMatchVoteMenu.m_Votes[Player->entindex()].VoteMap[Item.Info] = true;

			// Get Players
			auto Players = gMatchUtil.GetPlayers(true, true);

			// Needed votes
			auto VotesNeed = (Players.size() * VOTE_MENU_PERCENTAGE);

			// Get Vote Count
			auto VoteCount = 0;

			// Loop in game players
			for (const auto& Temp : Players)
			{
				// If has voted on that map
				if (gMatchVoteMenu.m_Votes[Temp->entindex()].VoteMap[Item.Info])
				{
					// Count vote
					VoteCount++;
				}
			}

			// Vote Map Progress to change map
			auto VoteProgress = (float)((VoteCount * 100) / VotesNeed);

			// If need more votes to change map
			if (VoteProgress < 100.0f)
			{
				gMatchUtil.SayText(nullptr, Player->entindex(), _T("\3%s\1 nomitated \4%s\1: %2.0f%% of votes to change map."), STRING(Player->edict()->v.netname), MapName.c_str(), VoteProgress);
				gMatchUtil.SayText(nullptr, Player->entindex(), _T("Say \3.vote\1 to nominate a map."));
			}
			else
			{
				gMatchChangeMap.ChangeMap(MapName, 5.0f, true);

				gMatchUtil.SayText(nullptr, Player->entindex(), _T("Changing map to \4%s\1..."), MapName.c_str());
			}
		}
		else
		{
			gMatchUtil.SayText(Player->edict(), Player->entindex(), _T("Already nominated \3%s\1..."), MapName.c_str());
		}
	}
}

// Vote Pause
bool CMatchVoteMenu::VotePause(CBasePlayer* Player)
{
	// If player is in game
	if (Player->m_iTeam == TERRORIST || Player->m_iTeam == CT)
	{
		// If vote state is not running
		if (gMatchBot.GetState() == STATE_FIRST_HALF || gMatchBot.GetState() == STATE_SECOND_HALF || gMatchBot.GetState() == STATE_OVERTIME)
		{
			// If has CS GameRules
			if (g_pGameRules)
			{
				// If is not in freeze time period
				if (!CSGameRules()->m_bFreezePeriod)
				{
					// If player not voted yet
					if (!this->m_Votes[Player->entindex()].VotePause[Player->m_iTeam])
					{
						// Add Vote
						this->m_Votes[Player->entindex()].VotePause[Player->m_iTeam] = true;

						// Get Players
						auto Players = gMatchUtil.GetPlayers(Player->m_iTeam, true);

						// Needed votes
						auto VotesNeed = (Players.size() * VOTE_MENU_PERCENTAGE);

						// Get Vote Count
						auto VoteCount = 0;

						// Loop Players in team
						for (const auto & Temp : Players)
						{
							// Count Vote if is true
							if (this->m_Votes[Temp->entindex()].VotePause[Temp->m_iTeam])
							{
								VoteCount++;
							}
						}

						// Vote Map Progress to change map
						auto VoteProgress = (float)((VoteCount * 100) / VotesNeed);

						// If need more votes to change map
						if (VoteProgress < 100.0f)
						{
							gMatchUtil.SayText(nullptr, Player->entindex(), _T("\3%s\1 from \3%s\1 voted for pause match: %2.0f%% of votes to pause match."), STRING(Player->edict()->v.netname), gMatchBot.GetTeam(Player->m_iTeam, false), VoteProgress);
							gMatchUtil.SayText(nullptr, Player->entindex(), _T("Say \3.vote\1 to vote for a pause."));
						}
						else
						{
							// Pause Match
							gMatchPause.Init(nullptr);
						}
					}
					else
					{
						gMatchUtil.SayText(Player->edict(), Player->entindex(), _T("You already voted to pause the game."));
					}

					return true;
				}
			}
		}
	}

	// Failed command
	gMatchUtil.SayText(Player->edict(), PRINT_TEAM_DEFAULT, _T("Unable to use this command now."));

	return false;
}

// Vote Restart
bool CMatchVoteMenu::VoteRestart(CBasePlayer* Player)
{
	// If player is in game
	if (Player->m_iTeam == TERRORIST || Player->m_iTeam == CT)
	{
		// Get Match BOT State
		auto MatchState = gMatchBot.GetState();

		// If state is valid
		if (MatchState == STATE_FIRST_HALF)
		{
			// If not voted to restart this state
			if (!this->m_Votes[Player->entindex()].VoteRestart[MatchState])
			{
				// Set player vote on this state to true
				this->m_Votes[Player->entindex()].VoteRestart[MatchState] = true;

				// Get Players in game
				auto Players = gMatchUtil.GetPlayers(true, true);

				// Needed votes
				auto VotesNeed = (Players.size() * VOTE_MENU_PERCENTAGE);

				// Get Vote Count
				auto VoteCount = 0;

				// Loop in game Players
				for (const auto& Temp : Players)
				{
					// If has voted to restart this state
					if (this->m_Votes[Temp->entindex()].VoteRestart[MatchState])
					{
						VoteCount++;
					}
				}

				// Vote Restart Progress to restart current period
				auto VoteProgress = (float)((VoteCount * 100) / VotesNeed);

				// If need more votes to restart period
				if (VoteProgress < 100.0f)
				{
					gMatchUtil.SayText(nullptr, Player->entindex(), _T("\3%s\1 voted to restart \3%s\1: %2.0f%% of votes to restart match."), STRING(Player->edict()->v.netname), gMatchBot.GetState(MatchState), VoteProgress);
					gMatchUtil.SayText(nullptr, Player->entindex(), _T("Say \3.vote\1 to restart match."), gMatchBot.GetState(MatchState));
				}
				else
				{
					// Send Mesasge
					gMatchUtil.SayText(nullptr, PRINT_TEAM_DEFAULT, _T("Restarting match.."), gMatchBot.GetState(MatchState));

					// Restart Match
					gMatchBot.RestartMatch(nullptr);
				}
			}
			else
			{
				gMatchUtil.SayText(Player->edict(), Player->entindex(), _T("You already voted to retart match."), gMatchBot.GetState(MatchState));
			}

			return true;
		}
	}

	// Failed command
	gMatchUtil.SayText(Player->edict(), PRINT_TEAM_DEFAULT, _T("Unable to use this command now."));

	return false;
}
