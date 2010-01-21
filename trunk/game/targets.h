#ifndef UFOATTACK_TARGETS_INCLUDED
#define UFOATTACK_TARGETS_INCLUDED

// Note that this structure gets copied POD style.
//
// Terran is enemy of Alien and vice versa. Civs aren't
// counted, which means they have to be queried and 
// don't impact reaction fire.
class Targets
{
public:
	Targets( const Unit* units ) { Clear(); base = units; }

	void Clear() {
		targets.ClearAll();
		teamTargets.ClearAll();
		memset( teamCount, 0, 9*sizeof(int) );
	}

	int Team( int id ) const {
		if ( id >= TERRAN_UNITS_START && id < TERRAN_UNITS_END )
			return Unit::SOLDIER;
		else if ( id >= CIV_UNITS_START && id < CIV_UNITS_END ) 
			return Unit::CIVILIAN;
		else if ( id >= ALIEN_UNITS_START && id < ALIEN_UNITS_END ) 
			return Unit::ALIEN;
		else { 
			GLASSERT( 0 ); 
			return 0;
		}
	}

	int Team( const Unit* unit ) const {
		return Team( unit - base );
	}

	void Set( int viewer, int target )		{ 
		targets.Set( viewer, target, 0 );
		if ( !teamTargets.IsSet( Team( viewer ), target ) ) 
			teamCount[ Team( viewer ) ][ Team( target ) ] += 1;
		teamTargets.Set( Team( viewer ), target );
	}

	int CanSee( int viewer, int target ) const	{
		return targets.IsSet( viewer, target, 0 );
	}
	int CanSee( const Unit* viewer, const Unit* target ) const {
		return CanSee( viewer-base, target-base );
	}

	int TeamCanSee( int viewerTeam, int target ) const {
		return teamTargets.IsSet( viewerTeam, target, 0 );
	}
	int TeamCanSee( int viewerTeam, const Unit* target ) const {
		return TeamCanSee( viewerTeam, target-base );
	}

	int TotalTeamCanSee( int viewerTeam, int targetTeam ) const {
		return teamCount[ viewerTeam ][ targetTeam ];
	}

	int CalcTotalUnitCanSee( int viewer, int targetTeam ) const {
		int start[] = { TERRAN_UNITS_START, CIV_UNITS_START, ALIEN_UNITS_START };
		int end[]   = { TERRAN_UNITS_END, CIV_UNITS_END, ALIEN_UNITS_END };
		int count = 0;
		GLASSERT( targetTeam >= 0 && targetTeam < 3 );
		for( int i=start[targetTeam]; i<end[targetTeam]; ++i ) {
			if ( targets.IsSet( viewer, i, 0 ) )
				++count;
		}
		return count;
	}

private:
	const Unit* base;
	grinliz::BitArray< MAX_UNITS, MAX_UNITS, 1 > targets;
	grinliz::BitArray< 3, MAX_UNITS, 1 > teamTargets;
	int teamCount[3][3];
};


#endif // UFOATTACK_TARGETS_INCLUDED