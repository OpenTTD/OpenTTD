#include "stdafx.h"
#include "ttd.h"


#define GETNUM(x, y) (((uint16)(seed >> x) * (y))>>16)

static void AppendPart(byte **buf, int num, const char *names)
{
	byte *s;

	while (--num>=0) {
		do names++; while (names[-1]);
	}
	
	for(s=*buf; (*s++ = *names++) != 0;) {}
	*buf = s - 1;
}

#define MK(x) x "\x0"

#define NUM_ENGLISH_1 4
static const char english_1[] = 
	MK("Great ")
	MK("Little ")
	MK("New ")
	MK("Fort ")
;

#define NUM_ENGLISH_2 26 
static const char english_2[] =
	MK("Wr")
	MK("B")
	MK("C")
	MK("Ch")
	MK("Br")
	MK("D")
	MK("Dr")
	MK("F")
	MK("Fr")
	MK("Fl")
	MK("G")
	MK("Gr")
	MK("H")
	MK("L")
	MK("M")
	MK("N")
	MK("P")
	MK("Pr")
	MK("Pl")
	MK("R")
	MK("S")
	MK("S")
	MK("Sl")
	MK("T")
	MK("Tr")
	MK("W")
;

#define NUM_ENGLISH_3 8
static const char english_3[] = 
	MK("ar")
	MK("a")
	MK("e")
	MK("in")
	MK("on")
	MK("u")
	MK("un")
	MK("en")
;

#define NUM_ENGLISH_4 7
static const char english_4[] = 
	MK("n")
	MK("ning")
	MK("ding")
	MK("d")
	MK("")
	MK("t")
	MK("fing")
;

#define NUM_ENGLISH_5 23
static const char english_5[] = 
	MK("ville")
	MK("ham")
	MK("field")
	MK("ton")
	MK("town")
	MK("bridge")
	MK("bury")
	MK("wood")
	MK("ford")
	MK("hall")
	MK("ston")
	MK("way")
	MK("stone")
	MK("borough")
	MK("ley")
	MK("head")
	MK("bourne")
	MK("pool")
	MK("worth")
	MK("hill")
	MK("well")
	MK("hattan")
	MK("burg")
;

#define NUM_ENGLISH_6 9
static const char english_6[] = 
	MK("-on-sea")
	MK(" Bay")
	MK(" Market")
	MK(" Cross")
	MK(" Bridge")
	MK(" Falls")
	MK(" City")
	MK(" Ridge")
	MK(" Springs")
;

static byte MakeEnglishTownName(byte *buf, uint32 seed)
{
	int i;
	byte result;
	byte *start;

	i = GETNUM(0, 54) - 50;
	if (i >= 0)
		AppendPart(&buf, i, english_1);
	
	start = buf;

	AppendPart(&buf, GETNUM(4, NUM_ENGLISH_2), english_2);
	AppendPart(&buf, GETNUM(7, NUM_ENGLISH_3), english_3);
	AppendPart(&buf, GETNUM(10, NUM_ENGLISH_4), english_4);
	AppendPart(&buf, GETNUM(13, NUM_ENGLISH_5), english_5);

	i = GETNUM(15, NUM_ENGLISH_6 + 60) - 60;

	result = 0;
	
	if (i >= 0) {
		if (i <= 1) result = NG_EDGE;
		AppendPart(&buf, i, english_6);
	}

	if (start[0]=='C' && (start[1] == 'e' || start[1] == 'i'))
		start[0] = 'K'; 

	/* FIXME: skip the banned words thing for now 
	only replacing "Cunt" with "Cult"   */
	if (start[0]=='C' && start[1] == 'u' && start[2] == 'n' && start[3] == 't')
		start[2] = 'l';

	return result;
}

#define NUM_AUSTRIAN_A1 6
static const char austrian_a1[] = 
	MK("Bad ")
	MK("Deutsch ")
	MK("Gross ")
	MK("Klein ")
	MK("Markt ")
	MK("Maria ")
;

#define NUM_AUSTRIAN_A2 42
static const char austrian_a2[] = 
	MK("Aus")
	MK("Alten")
	MK("Braun")
	MK("Vösl")
	MK("Mittern")
	MK("Nuss")
	MK("Neu")
	MK("Walters")
	MK("Breiten")
	MK("Eisen")
	MK("Feld")
	MK("Mittern")
	MK("Gall")
	MK("Obern")
	MK("Grat")
	MK("Heiligen")
	MK("Hof")
	MK("Holla")
	MK("Stein")
	MK("Eber")
	MK("Eggen")
	MK("Enzers")
	MK("Frauen")
	MK("Herren")
	MK("Hof")
	MK("Hütt")
	MK("Kaisers")
	MK("Königs")
	MK("Knittel")
	MK("Lang")
	MK("Ober")
	MK("Ollers")
	MK("Pfaffen")
	MK("Potten")
	MK("Salz")
	MK("Schwarz")
	MK("Stocker")
	MK("Unter")
	MK("Utten")
	MK("Vösen")
	MK("Vill")
	MK("Weissen")
;

#define NUM_AUSTRIAN_A3 16
static const char austrian_a3[] = 
	MK("see")
	MK("bach")
	MK("dorf")
	MK("ach")
	MK("stein")
	MK("hofen")
	MK("au")
	MK("ach")
	MK("kirch")
	MK("kirchen")
	MK("kreuz")
	MK("brunn")
	MK("siedl")
	MK("markt")
	MK("wang")
	MK("haag")
;

#define NUM_AUSTRIAN_A4 12
static const char austrian_a4[] =
	MK("Bruck")
	MK("Brunn")
	MK("Gams")
	MK("Grein")
	MK("Ried")
	MK("Faak")
	MK("Zell")
	MK("Spital")
	MK("Kirchberg")
	MK("Saal")
	MK("Taferl")
	MK("Wald")
;

#define NUM_AUSTRIAN_A5 2
static const char austrian_a5[] =
	MK("St. ")
	MK("Sankt ")
;

#define NUM_AUSTRIAN_A6 20
static const char austrian_a6[] =
	MK("Aegyd")
	MK("Andrä")
	MK("Georgen")
	MK("Jakob")
	MK("Johann")
	MK("Leonhard")
	MK("Marein")
	MK("Lorenzen")
	MK("Margarethen")
	MK("Martin")
	MK("Michael")
	MK("Nikolai")
	MK("Oswald")
	MK("Peter")
	MK("Pölten")
	MK("Stefan")
	MK("Stephan")
	MK("Thomas")
	MK("Veit")
	MK("Wolfgang")
;

#define NUM_AUSTRIAN_F1 2
static const char austrian_f1[] =
	MK(" an der ")
	MK(" ob der ")
;

#define NUM_AUSTRIAN_F2 13
static const char austrian_f2[] =
	MK("Donau")
	MK("Steyr")
	MK("Lafnitz")
	MK("Leitha")
	MK("Thaya")
	MK("Gail")
	MK("Drau")
	MK("Salzach")
	MK("Ybbs")
	MK("Traisen")
	MK("Enns")
	MK("Mur")
	MK("Ill")
;

#define NUM_AUSTRIAN_B1 1
static const char austrian_b1[] =
	MK(" am ")
;

#define NUM_AUSTRIAN_B2 10
static const char austrian_b2[] =
	MK("Brenner")
	MK("Dachstein")
	MK("Gebirge")
	MK("Grossglockner")
	MK("Hausruck")
	MK("Semmering")
	MK("Wagram")
	MK("Wechsel")
	MK("Wilden Kaiser")
	MK("Ziller")
;

static byte MakeAustrianTownName(byte *buf, uint32 seed)
{
	int i, j=0;

	// Bad, Maria, Gross, ...
	i = GETNUM(0, NUM_AUSTRIAN_A1 + 15) - 15;
	if (i >= 0) AppendPart(&buf, i, austrian_a1);

	i = GETNUM(4, 6);
	if(i >= 4) {
	  // Kaisers-kirchen
	  AppendPart(&buf, GETNUM( 7, NUM_AUSTRIAN_A2), austrian_a2);
	  AppendPart(&buf, GETNUM(13, NUM_AUSTRIAN_A3), austrian_a3);
	}
	else if(i >= 2) {
	  // St. Johann
	  AppendPart(&buf, GETNUM( 7, NUM_AUSTRIAN_A5), austrian_a5);
	  AppendPart(&buf, GETNUM( 9, NUM_AUSTRIAN_A6), austrian_a6);
	  j=1; // More likely to have a " an der " or " am "
	}
        else {
	  // Zell
	  AppendPart(&buf, GETNUM( 7, NUM_AUSTRIAN_A4), austrian_a4);
	}

	i = GETNUM(1, 6);
	if(i >= 4-j) {
	  // an der Donau (rivers)
	  AppendPart(&buf, GETNUM(4, NUM_AUSTRIAN_F1), austrian_f1);
	  AppendPart(&buf, GETNUM(5, NUM_AUSTRIAN_F2), austrian_f2);
	}
	else if(i >= 2-j) {
	  // am Dachstein (mountains)
	  AppendPart(&buf, GETNUM(4, NUM_AUSTRIAN_B1), austrian_b1);
	  AppendPart(&buf, GETNUM(5, NUM_AUSTRIAN_B2), austrian_b2);
	}

	return 0;
}

#define NUM_GERMAN_1 114
#define NUM_GERMAN_1_HARDCODED 21 
static const char german_1[] = 
	/* hardcoded names */
	MK("Berlin")
	MK("Bonn")
	MK("Bremen")
	MK("Cottbus")
	MK("Chemnitz")
	MK("Dortmund")
	MK("Dresden")
	MK("Erfurt")
	MK("Erlangen")
	MK("Essen")
	MK("Fulda")
	MK("Gera")
	MK("Kassel")
	MK("Kiel")
	MK("Köln")
	MK("Lübeck")
	MK("Magdeburg")
	MK("München")
	MK("Potsdam")
	MK("Stuttgart")
	MK("Wiesbaden")
	/* randomizer names */
	MK("Alb")
	MK("Als")
	MK("Ander")
	MK("Arns")
	MK("Bruns")
	MK("Bam")
	MK("Biele")
	MK("Cloppen")
	MK("Co")
	MK("Duis")
	MK("Düssel")
	MK("Dannen")
	MK("Elb")
	MK("Els")
	MK("Elster")
	MK("Eichen")
	MK("Ems")
	MK("Fahr")
	MK("Falken")
	MK("Flens")
	MK("Frank")
	MK("Frei")
	MK("Freuden")
	MK("Fried")
	MK("Fürsten")
	MK("Hahn")
	MK("Ham")
	MK("Harz")
	MK("Heidel")
	MK("Hers")
	MK("Herz")
	MK("Holz")
	MK("Hildes")
	MK("Inns")
	MK("Ilsen")
	MK("Ingols")
	MK("Kel")
	MK("Kies")
	MK("Korn")
	MK("Kor")
	MK("Kreuz")
	MK("Kulm")
	MK("Langen")
	MK("Lim")
	MK("Lohr")
	MK("Lüne")
	MK("Mel")
	MK("Michels")
	MK("Mühl")
	MK("Naum")
	MK("Nest")
	MK("Nord")
	MK("Nort")
	MK("Nien")
	MK("Nidda")
	MK("Nieder")
	MK("Nürn")
	MK("Ober")
	MK("Offen")
	MK("Osna")
	MK("Olden")
	MK("Ols")
	MK("Oranien")
	MK("Pader")
	MK("Quedlin")
	MK("Quer")
	MK("Ravens")
	MK("Regens")
	MK("Rott")
	MK("Ros")
	MK("Rüssels")
	MK("Saal")
	MK("Saar")
	MK("Salz")
	MK("Schöne")
	MK("Schwein")
	MK("Sonder")
	MK("Sonnen")
	MK("Stein")
	MK("Strals")
	MK("Straus")
	MK("Süd")
	MK("Ton")
	MK("Unter")
	MK("Ur")
	MK("Vor")
	MK("Wald")
	MK("War")
	MK("Wert")
	MK("Wester")
	MK("Witten")
	MK("Wolfs")
	MK("Würz")
;

#define NUM_GERMAN_2 16
static const char german_2[] =
	MK("bach")
	MK("berg")
	MK("brück")
	MK("brücken")
	MK("burg")
	MK("dorf")
	MK("feld")
	MK("furt")
	MK("hausen")
	MK("haven")
	MK("heim")
	MK("horst")
	MK("mund")
	MK("münster")
	MK("stadt")
	MK("wald")
;

#define NUM_GERMAN_3 5
static const char german_3[] =
	MK(" an der ")
	MK(" am ")
	MK("Bad ")
	MK("Klein ")
	MK("Neu ")
;

#define NUM_GERMAN_4 6
#define NUM_GERMAN_4_PRECHANGE 5
static const char german_4[] =
	/* use "an der" */
	MK("Oder")
	MK("Spree")
	MK("Donau")
	MK("Saale")
	MK("Elbe")
	/* use "am" */
	MK("Main")
	
;

static byte MakeGermanTownName(byte *buf, uint32 seed)
{
	int i;
	int ext;
	
	ext=GETNUM(7, 28); /* Extension - Prefix / Suffix */

	if ((ext==12) || (ext==19)) {
		i=GETNUM(2,NUM_GERMAN_3-2);
		AppendPart(&buf, 2+i, german_3);
	}


	i=GETNUM(3,NUM_GERMAN_1);

	AppendPart(&buf, i, german_1);

	if (i>NUM_GERMAN_1_HARDCODED-1) {
		AppendPart(&buf, GETNUM(5, NUM_GERMAN_2), german_2);
	}

	if (ext==24) {
		i=GETNUM(9,NUM_GERMAN_4);

		if (i<=NUM_GERMAN_4_PRECHANGE-1) {
			AppendPart(&buf, 0, german_3);
			AppendPart(&buf, i, german_4);
		} else {
			AppendPart(&buf, 1, german_3);
			AppendPart(&buf, i, german_4);
		}
	}

	return 0;
}

#define NUM_SPANISH_1 86
static const char spanish_1[] = 
	MK("Caracas")
	MK("Maracay")
	MK("Maracaibo")
	MK("Velencia")
	MK("El Dorado")
	MK("Morrocoy")
	MK("Cata")
	MK("Cataito")
	MK("Ciudad Bolivar")
	MK("Barquisimeto")
	MK("Merida")
	MK("Puerto Ordaz")
	MK("Santa Elena")
	MK("San Juan")
	MK("San Luis")
	MK("San Rafael")
	MK("Santiago")
	MK("Barcelona")
	MK("Barinas")
	MK("San Cristobal")
	MK("San Fransisco")
	MK("San Martin")
	MK("Guayana")
	MK("San Carlos")
	MK("El Limon")
	MK("Coro")
	MK("Corocoro")
	MK("Puerto Ayacucho")
	MK("Elorza")
	MK("Arismendi")
	MK("Trujillo")
	MK("Carupano")
	MK("Anaco")
	MK("Lima")
	MK("Cuzco")
	MK("Iquitos")
	MK("Callao")
	MK("Huacho")
	MK("Camana")
	MK("Puerto Chala")
	MK("Santa Cruz")
	MK("Quito")
	MK("Cuenca")
	MK("Huacho")
	MK("Tulcan")
	MK("Esmereldas")
	MK("Ibarra")
	MK("San Lorenzo")
	MK("Macas")
	MK("Morana")
	MK("Machala")
	MK("Zamora")
	MK("Latacunga")
	MK("Tena")
	MK("Cochabamba")
	MK("Ascencion")
	MK("Magdalena")
	MK("Santa Ana")
	MK("Manoa")
	MK("Sucre")
	MK("Oruro")
	MK("Uyuni")
	MK("Potosi")
	MK("Tupiza")
	MK("La Quiaca")
	MK("Yacuiba")
	MK("San Borja")
	MK("Fuerte Olimpio")
	MK("Fortin Esteros")
	MK("Campo Grande")
	MK("Bogota")
	MK("El Banco")
	MK("Zaragosa")
	MK("Neiva")
	MK("Mariano")
	MK("Cali")
	MK("La Palma")
	MK("Andoas")
	MK("Barranca")
	MK("Montevideo")
	MK("Valdivia")
	MK("Arica")
	MK("Temuco")
	MK("Tocopilla")
	MK("Mendoza")
	MK("Santa Rosa");

static byte MakeSpanishTownName(byte *buf, uint32 seed)
{
	AppendPart(&buf, GETNUM(0, NUM_SPANISH_1), spanish_1);
	return 0;	
}

#define NUM_FRENCH_1 70
static const char french_1[] = 
	MK("Agincourt")
	MK("Lille")
	MK("Dinan")
	MK("Aubusson")
	MK("Rodez")
	MK("Bergerac")
	MK("Bordeaux")
	MK("Bayonne")
	MK("Montpellier")
	MK("Montelimar")
	MK("Valence")
	MK("Digne")
	MK("Nice")
	MK("Cannes")
	MK("St. Tropez")
	MK("Marseilles")
	MK("Narbonne")
	MK("Sète") 
	MK("Aurillac")
	MK("Gueret")
	MK("Le Creusot")
	MK("Nevers")
	MK("Auxerre")
	MK("Versailles")
	MK("Meaux")
	MK("Châlons")
	MK("Compiègne")
	MK("Metz")
	MK("Chaumont")
	MK("Langres")
	MK("Bourg")
	MK("Lyons")
	MK("Vienne")
	MK("Grenoble")
	MK("Toulon")
	MK("Rennes")
	MK("Le Mans")
	MK("Angers")
	MK("Nantes")
	MK("Châteauroux")
	MK("Orléans")
	MK("Lisieux")
	MK("Cherbourg")
	MK("Morlaix")
	MK("Cognac")
	MK("Agen")
	MK("Tulle")
	MK("Blois")
	MK("Troyes")
	MK("Charolles")
	MK("Grenoble")
	MK("Chambéry")
	MK("Tours")
	MK("St. Brieuc")
	MK("St. Malo")
	MK("La Rochelle")
	MK("St. Flour")
	MK("Le Puy")
	MK("Vichy")
	MK("St. Valery")
	MK("Beaujolais")
	MK("Narbonne")
	MK("Albi")
	MK("St. Valery")
	MK("Biarritz")
	MK("Béziers")
	MK("Nîmes")
	MK("Chamonix")
	MK("Angoulème")
	MK("Alençon");

static byte MakeFrenchTownName(byte *buf, uint32 seed)
{
	AppendPart(&buf, GETNUM(0, NUM_FRENCH_1), french_1);
	return 0;
}

static byte MakeAmericanTownName(byte *buf, uint32 seed)
{
	// make american town names equal to english for now.
	return MakeEnglishTownName(buf, seed);
}

#define NUM_SILLY_1 88
static const char silly_1[] = 
	MK("Binky")
	MK("Blubber")
	MK("Bumble")
	MK("Crinkle")
	MK("Crusty")
	MK("Dangle")
	MK("Dribble")
	MK("Flippety")
	MK("Google")
	MK("Muffin")

	MK("Nosey")
	MK("Pinker")
	MK("Quack")
	MK("Rumble")
	MK("Sleepy")
	MK("Sliggles")
	MK("Snooze")
	MK("Teddy")
	MK("Tinkle")
	MK("Twister")

	MK("Pinker")
	MK("Hippo")
	MK("Itchy")
	MK("Jelly")
	MK("Jingle")
	MK("Jolly")
	MK("Kipper")
	MK("Lazy")
	MK("Frogs")
	MK("Mouse")

	MK("Quack")
	MK("Cheeky")
	MK("Lumpy")
	MK("Grumpy")
	MK("Mangle")
	MK("Fiddle")
	MK("Slugs")
	MK("Noodles")
	MK("Poodles")
	MK("Shiver")

	MK("Rumble")
	MK("Pixie")
	MK("Puddle")
	MK("Riddle")
	MK("Rattle")
	MK("Rickety")
	MK("Waffle")
	MK("Sagging")
	MK("Sausage")
	MK("Egg")

	MK("Sleepy")
	MK("Scatter")
	MK("Scramble")
	MK("Silly")
	MK("Simple")
	MK("Trickle")
	MK("Slippery")
	MK("Slimey")
	MK("Slumber")
	MK("Soggy")

	MK("Sliggles")
	MK("Splutter")
	MK("Sulky")
	MK("Swindle")
	MK("Swivel")
	MK("Tasty")
	MK("Tangle")
	MK("Toggle")
	MK("Trotting")
	MK("Tumble")

	MK("Snooze")
	MK("Water")
	MK("Windy")
	MK("Amble")
	MK("Bubble")
	MK("Cheery")
	MK("Cheese")
	MK("Cockle")
	MK("Cracker")
	MK("Crumple")

	MK("Teddy")
	MK("Evil")
	MK("Fairy")
	MK("Falling")
	MK("Fishy")
	MK("Fizzle")
	MK("Frosty")
	MK("Griddle")
;

#define NUM_SILLY_2 15
static const char silly_2[] = 
	MK("ton")
	MK("bury")
	MK("bottom")
	MK("ville")
	MK("well")
	MK("weed")
	MK("worth")
	MK("wig")
	MK("wick")
	MK("wood")
	
	MK("pool")
	MK("head")
	MK("burg")
	MK("gate")
	MK("bridge")
;


static byte MakeSillyTownName(byte *buf, uint32 seed)
{		
	AppendPart(&buf, GETNUM(0, NUM_SILLY_1), silly_1);
	AppendPart(&buf, GETNUM(16, NUM_SILLY_2),silly_2);
	return 0;
}


#define NUM_SWEDISH_1 4
static const char swedish_1[] =
	MK("Gamla ")
	MK("Lilla ")
	MK("Nya ")
	MK("Stora ");

#define NUM_SWEDISH_2 38
static const char swedish_2[] =
	MK("Boll")
	MK("Bor")
	MK("Ed")
	MK("En")
	MK("Erik")
	MK("Es")
	MK("Fin")
	MK("Fisk")
	MK("Grön")
	MK("Hag")
	MK("Halm")
	MK("Karl")
	MK("Kram")
	MK("Kung")
	MK("Land")
	MK("Lid")
	MK("Lin")
	MK("Mal")
	MK("Malm")
	MK("Marie")
	MK("Ner")
	MK("Norr")
	MK("Oskar")
	MK("Sand")
	MK("Skog")
	MK("Stock")
	MK("Stor")
	MK("Ström")
	MK("Sund")
	MK("Söder")
	MK("Tall")
	MK("Tratt")
	MK("Troll")
	MK("Upp")
	MK("Var")
	MK("Väster")
	MK("Ängel")
	MK("Öster");

#define NUM_SWEDISH_2A 42
static const char swedish_2a[] =
	MK("B")
	MK("Br")
	MK("D")
	MK("Dr")
	MK("Dv")
	MK("F")
	MK("Fj")
	MK("Fl")
	MK("Fr")
	MK("G")
	MK("Gl")
	MK("Gn")
	MK("Gr")
	MK("H")
	MK("J")
	MK("K")
	MK("Kl")
	MK("Kn")
	MK("Kr")
	MK("Kv")
	MK("L")
	MK("M")
	MK("N")
	MK("P")
	MK("Pl")
	MK("Pr")
	MK("R")
	MK("S")
	MK("Sk")
	MK("Skr")
	MK("Sl")
	MK("Sn")
	MK("Sp")
	MK("Spr")
	MK("St")
	MK("Str")
	MK("Sv")
	MK("T")
	MK("Tr")
	MK("Tv")
	MK("V")
	MK("Vr");

#define NUM_SWEDISH_2B 9
static const char swedish_2b[] =
	MK("a")
	MK("e")
	MK("i")
	MK("o")
	MK("u")
	MK("y")
	MK("å")
	MK("ä")
	MK("ö");

#define NUM_SWEDISH_2C 26
static const char swedish_2c[] =
	MK("ck")
	MK("d")
	MK("dd")
	MK("g")
	MK("gg")
	MK("l")
	MK("ld")
	MK("m")
	MK("n")
	MK("nd")
	MK("ng")
	MK("nn")
	MK("p")
	MK("pp")
	MK("r")
	MK("rd")
	MK("rk")
	MK("rp")
	MK("rr")
	MK("rt")
	MK("s")
	MK("sk")
	MK("st")
	MK("t")
	MK("tt")
	MK("v");

#define NUM_SWEDISH_3 32
static const char swedish_3[] =
	MK("arp")
	MK("berg")
	MK("boda")
	MK("borg")
	MK("bro")
	MK("bukten")
	MK("by")
	MK("byn")
	MK("fors")
	MK("hammar")
	MK("hamn")
	MK("holm")
	MK("hus")
	MK("hättan")
	MK("kulle")
	MK("köping")
	MK("lund")
	MK("löv")
	MK("sala")
	MK("skrona")
	MK("slätt")
	MK("spång")
	MK("stad")
	MK("sund")
	MK("svall")
	MK("svik")
	MK("såker")
	MK("udde")
	MK("valla")
	MK("viken")
	MK("älv")
	MK("ås");

static byte MakeSwedishTownName(byte *buf, uint32 seed)
{
	int i;

	i = GETNUM(0, 50 + NUM_SWEDISH_1) - 50;
	if (i >= 0) AppendPart(&buf, i, swedish_1);

	if (GETNUM(4, 5) >= 3)
		AppendPart(&buf, GETNUM(7, NUM_SWEDISH_2), swedish_2);
	else {
		AppendPart(&buf, GETNUM(7, NUM_SWEDISH_2A), swedish_2a);
		AppendPart(&buf, GETNUM(10, NUM_SWEDISH_2B), swedish_2b);
		AppendPart(&buf, GETNUM(13, NUM_SWEDISH_2C), swedish_2c);
	}

	AppendPart(&buf, GETNUM(16, NUM_SWEDISH_3), swedish_3);

	return 0;
}


#define NUM_DUTCH_1 8
static const char dutch_1[] =
	MK("Nieuw ")
	MK("Oud ")
	MK("Groot ")
	MK("Zuid ")
	MK("Noord ")
	MK("Oost ")
	MK("West ")
	MK("Klein ");

#define NUM_DUTCH_2 57
static const char dutch_2[] = 
	MK("Hoog")
	MK("Laag")
	MK("Klein")
	MK("Groot")
	MK("Noorder")
	MK("Noord")
	MK("Zuider")
	MK("Zuid")
	MK("Ooster")
	MK("Oost")
	MK("Wester")
	MK("West")
	MK("Hoofd")
	MK("Midden")
	MK("Eind")
	MK("Amster")
	MK("Amstel")
	MK("Dord")
	MK("Rotter")
	MK("Haar")
	MK("Til")
	MK("Enk")
	MK("Dok")
	MK("Veen")
	MK("Leidsch")
	MK("Lely")
	MK("En")
	MK("Kaats")
	MK("U")
	MK("Maas")
	MK("Mar")
	MK("Bla")
	MK("Al")
	MK("Alk")
	MK("Eer")
	MK("Drie")
	MK("Ter")
	MK("Groes")
	MK("Goes")
	MK("Soest")
	MK("Coe")
	MK("Uit")
	MK("Zwaag")
	MK("Hellen")
	MK("Slie")
	MK("IJ")
	MK("Grubben")
	MK("Groen")
	MK("Lek")
	MK("Ridder")
	MK("Schie")
	MK("Olde")
	MK("Roose")
	MK("Haar")
	MK("Til")
	MK("Loos")
	MK("Hil");

#define NUM_DUTCH_3 20
static const char dutch_3[] = 
	MK("Drog")
	MK("Nat")
	MK("Valk")
	MK("Bob")
	MK("Dedem")
	MK("Kollum")
	MK("Best")
	MK("Hoend")
	MK("Leeuw")
	MK("Graaf")
	MK("Uithuis")
	MK("Purm")
	MK("Hard")
	MK("Hell")
	MK("Werk")
	MK("Spijk")
	MK("Vink")
	MK("Wams")
	MK("Heerhug")
	MK("Koning");
	

#define NUM_DUTCH_4 6
static const char dutch_4[] = 
	MK("e")
	MK("er")
	MK("el")
	MK("en")
	MK("o")
	MK("s");

#define NUM_DUTCH_5 56
static const char dutch_5[] = 
	MK("stad")
	MK("vorst")
	MK("dorp")
	MK("dam")
	MK("beek")
	MK("doorn")
	MK("zijl")
	MK("zijlen")
	MK("lo")
	MK("muiden")
	MK("meden")
	MK("vliet")
	MK("nisse")
	MK("daal")
	MK("vorden")
	MK("vaart")
	MK("mond")
	MK("zaal")
	MK("water")
	MK("duinen")
	MK("heuvel")
	MK("geest")
	MK("kerk")
	MK("meer")
	MK("maar")
	MK("hoorn")
	MK("rade")
	MK("wijk")
	MK("berg")
	MK("heim")
	MK("sum")
	MK("richt")
	MK("burg")
	MK("recht")
	MK("drecht")
	MK("trecht")
	MK("tricht")
	MK("dricht")
	MK("lum")
	MK("rum")
	MK("halen")
	MK("oever")
	MK("wolde")
	MK("veen")
	MK("hoven")
	MK("gast")
	MK("kum")
	MK("hage")
	MK("dijk")
	MK("zwaag")
	MK("pomp")
	MK("huizen")
	MK("bergen")
	MK("schede")
	MK("mere")
	MK("end");
	
static byte MakeDutchTownName(byte *buf, uint32 seed)
{
	int i;

	i = GETNUM(0, 50 + NUM_DUTCH_1) - 50;
	if (i >= 0) 
		AppendPart(&buf, i, dutch_1);

	i = GETNUM(6, 9);
	if(i > 4){
		AppendPart(&buf, GETNUM(9, NUM_DUTCH_2), dutch_2);
	} else {
		AppendPart(&buf, GETNUM(9, NUM_DUTCH_3), dutch_3);
		AppendPart(&buf, GETNUM(12, NUM_DUTCH_4), dutch_4);
	}
	AppendPart(&buf, GETNUM(15, NUM_DUTCH_5), dutch_5);

	return 0;
}

#define NUM_FINNISH_1 25
static const char finnish_1[] = 
	MK("Aijala")
	MK("Kisko")
	MK("Espoo")
	MK("Helsinki")
	MK("Tapiola")
	MK("Järvelä")
	MK("Lahti")
	MK("Kotka")
	MK("Hamina")
	MK("Loviisa")
	MK("Kouvola")
	MK("Tampere")
	MK("Kokkola")
	MK("Oulu")
	MK("Salo")
	MK("Malmi")
	MK("Pelto")
	MK("Koski")
	MK("Iisalmi")
	MK("Raisio")
	MK("Taavetti")
	MK("Joensuu")
	MK("Imatra")
	MK("Tapanila")
	MK("Pasila");
 
#define NUM_FINNISH_2a 26
static const char finnish_2a[] = 
	MK("Hiekka")
	MK("Haapa")
	MK("Mylly")
	MK("Kivi")
	MK("Lappeen")
	MK("Lohjan")
	MK("Savon")
	MK("Sauna")
	MK("Keri")
	MK("Uusi")
	MK("Vanha")
	MK("Lapin")
	MK("Kesä")
	MK("Kuusi")
	MK("Pelto")
	MK("Tuomi")
	MK("Pitäjän")
	MK("Terva")
	MK("Olki")
	MK("Heinä")
	MK("Kuusan")
	MK("Seinä")
	MK("Kemi")
	MK("Rova")
	MK("Martin")
	MK("Koivu");

#define NUM_FINNISH_2b 18
static const char finnish_2b[] = 
	MK("harju")
	MK("linna")
	MK("järvi")
	MK("kallio")
	MK("mäki")
	MK("nummi")
	MK("joki")
	MK("kylä")
	MK("lampi")
	MK("lahti")
	MK("metsä")
	MK("suo")
	MK("laakso")
	MK("niitty")
	MK("luoto")
	MK("hovi")
	MK("ranta")
	MK("koski");

static byte MakeFinnishTownName(byte *buf, uint32 seed)
{
// Select randomly if town name should consists of one or two parts.
	if (GETNUM(0, 15) >= 10)
		AppendPart(&buf, GETNUM(2, NUM_FINNISH_1), finnish_1); // One part
	else {
		AppendPart(&buf, GETNUM(2, NUM_FINNISH_2a), finnish_2a); // Two parts
		AppendPart(&buf, GETNUM(10, NUM_FINNISH_2b), finnish_2b);
	}
	return 0;
}

#define NUM_POLISH_1 11

static const char polish_1_m[] =
	MK("Wielki ")
	MK("Maly ")
	MK("Zly ")
	MK("Dobry ")
	MK("Nowy ")
	MK("Stary ")
	MK("Zloty ")
	MK("Zielony ")
	MK("Bialy ")
	MK("Modry ")
	MK("Debowy ")
	;

static const char polish_1_f[] =
	MK("Wielka ")
	MK("Mala ")
	MK("Zla ")
	MK("Dobra ")
	MK("Nowa ")
	MK("Stara ")
	MK("Zlota ")
	MK("Zielona ")
	MK("Biala ")
	MK("Modra ")
	MK("Debowa ")
	;

static const char polish_1_n[] =
	MK("Wielkie ")
	MK("Male ")
	MK("Zle ")
	MK("Dobre ")
	MK("Nowe ")
	MK("Stare ")
	MK("Zlote ")
	MK("Zielone ")
	MK("Biale ")
	MK("Modre ")
	MK("Debowe ")
	;

#define NUM_POLISH_2_O 34// single names
#define NUM_POLISH_2_M 48// masculine + pref/suf
#define NUM_POLISH_2_F 27// feminine + pref/suf
#define NUM_POLISH_2_N 29// 'it' + pref/suf

static const char polish_2[] =
//static const char polish_2_o[] =
MK("Frombork")
MK("Gniezno")
MK("Olsztyn")
MK("Torun")
MK("Bydgoszcz")
MK("Terespol")
MK("Krakow")
MK("Poznan")
MK("Wroclaw")
MK("Katowice")
MK("Cieszyn")
MK("Bytom")
MK("Frombork")
MK("Hel")
MK("Konin")
MK("Lublin")
MK("Malbork")
MK("Sopot")
MK("Sosnowiec")
MK("Gdansk")
MK("Gdynia")
MK("Sieradz")
MK("Sandomierz")
MK("Szczyrk")
MK("Szczytno")
MK("Szczecin")
MK("Zakopane")
MK("Szklarska Poreba")
MK("Bochnia")
MK("Golub-Dobrzyn")
MK("Chojnice")
MK("Ostrowiec")
MK("Otwock")
MK("Wolsztyn")
//;

//static const char polish_2_m[] =
MK("Jarocin")
MK("Gogolin")
MK("Tomaszow")
MK("Piotrkow")
MK("Lidzbark")
MK("Rypin")
MK("Radzymin")
MK("Wolomin")
MK("Pruszkow")
MK("Olsztynek")
MK("Rypin")
MK("Cisek")
MK("Krotoszyn")
MK("Stoczek")
MK("Lubin")
MK("Lubicz")
MK("Milicz")
MK("Targ")
MK("Ostrow")
MK("Ozimek")
MK("Puck")
MK("Rzepin")
MK("Siewierz")
MK("Stargard")
MK("Starogard")
MK("Turek")
MK("Tymbark")
MK("Wolsztyn")
MK("Strzepcz")
MK("Strzebielin")
MK("Sochaczew")
MK("Grebocin")
MK("Gniew")
MK("Lubliniec")
MK("Lubasz")
MK("Lutomiersk")
MK("Niemodlin")
MK("Przeworsk")
MK("Ursus")
MK("Tyczyn")
MK("Sztum")
MK("Szczebrzeszyn")
MK("Wolin")
MK("Wrzeszcz")
MK("Zgierz")
MK("Zieleniec")
MK("Drobin")
MK("Garwolin")
//;

//static const char polish_2_f[] =
MK("Szprotawa")
MK("Pogorzelica")
MK("Motlawa")
MK("Lubawa")
MK("Nidzica")
MK("Kruszwica")
MK("Bierawa")
MK("Brodnica")
MK("Chojna")
MK("Krzepica")
MK("Ruda")
MK("Rumia")
MK("Tuchola")
MK("Trzebinia")
MK("Ustka")
MK("Warszawa")
MK("Bobowa")
MK("Dukla")
MK("Krynica")
MK("Murowana")
MK("Niemcza")
MK("Zaspa")
MK("Zawoja")
MK("Wola")
MK("Limanowa")
MK("Rabka")
MK("Skawina")
MK("Pilawa")
//;

//static const char polish_2_n[] =
MK("Lipsko")
MK("Pilzno")
MK("Przodkowo")
MK("Strzelno")
MK("Susz")
MK("Jaworzno")
MK("Choszczno")
MK("Mogilno")
MK("Luzino")
MK("Miasto")
MK("Dziadowo")
MK("Kowalewo")
MK("Legionowo")
MK("Miastko")
MK("Zabrze")
MK("Zawiercie")
MK("Kochanowo")
MK("Miechucino")
MK("Mirachowo")
MK("Robakowo")
MK("Kosakowo")
MK("Borne")
MK("Braniewo")
MK("Sulinowo")
MK("Chmielno")
MK("Jastrzebie")
MK("Gryfino")
MK("Koronowo")
MK("Lubichowo")
MK("Opoczno")
;

#define NUM_POLISH_3 29
static const char polish_3_m[] =
MK(" Wybudowanie")
MK(" Swietokrzyski")
MK(" Gorski")
MK(" Morski")
MK(" Zdroj")
MK(" Wody")
MK(" Bajoro")
MK(" Krajenski")
MK(" Slaski")
MK(" Mazowiecki")
MK(" Pomorski")
MK(" Wielki")
MK(" Maly")
MK(" Warminski")
MK(" Mazurski")
MK(" Mniejszy")
MK(" Wiekszy")
MK(" Gorny")
MK(" Dolny")
MK(" Wielki")
MK(" Stary")
MK(" Nowy")
MK(" Wielkopolski")
MK(" Wzgorze")
MK(" Mosty")
MK(" Kujawski")
MK(" Malopolski")
MK(" Podlaski")
MK(" Lesny")
;
static const char polish_3_f[] = 
MK(" Wybudowanie")
MK(" Swietokrzyska")
MK(" Gorska")
MK(" Morska")
MK(" Zdroj")
MK(" Woda")
MK(" Bajoro")
MK(" Krajenska")
MK(" Slaska")
MK(" Mazowiecka")
MK(" Pomorska")
MK(" Wielka")
MK(" Mala")
MK(" Warminska")
MK(" Mazurska")
MK(" Mniejsza")
MK(" Wieksza")
MK(" Gorna")
MK(" Dolna")
MK(" Wielka")
MK(" Stara")
MK(" Nowa")
MK(" Wielkopolska")
MK(" Wzgorza")
MK(" Mosty")
MK(" Kujawska")
MK(" Malopolska")
MK(" Podlaska")
MK(" Lesna")
;
static const char polish_3_n[] =
MK(" Wybudowanie")
MK(" Swietokrzyskie")
MK(" Gorskie")
MK(" Morskie")
MK(" Zdroj")
MK(" Wody")
MK(" Bajoro")
MK(" Krajenskie")
MK(" Slaskie")
MK(" Mazowieckie")
MK(" Pomorskie")
MK(" Wielkie")
MK(" Male")
MK(" Warminskie ")
MK(" Mazurskie ")
MK(" Mniejsze")
MK(" Wieksze")
MK(" Gorne")
MK(" Dolne")
MK(" Wielkie")
MK(" Stare")
MK(" Nowe")
MK(" Wielkopolskie")
MK(" Wzgorze")
MK(" Mosty")
MK(" Kujawskie")
MK(" Malopolskie")
MK(" Podlaskie")
MK(" Lesne")
;

#define NUM_POLISH_2 NUM_POLISH_2_O + NUM_POLISH_2_M + NUM_POLISH_2_F + NUM_POLISH_2_N

static const char * const _polish_types[3][2] = {
	{polish_1_m, polish_3_m}, // masculine
	{polish_1_f, polish_3_f}, // feminine
	{polish_1_n, polish_3_n}, // neutral
};

static byte MakePolishTownName(byte *buf, uint32 seed)
{
	uint i, x;
	const char *const (*t)[2];

	// get a number ranging from 0 to all_towns
	i = GETNUM(0, NUM_POLISH_2);

	// single name
	if(i < NUM_POLISH_2_O) {
		AppendPart(&buf, i, polish_2);
		return 0;
	}

	// a suffix (12/20), a prefix (4/20), or nothing (4/20)
	x = GETNUM(5, 20);

	// no suffix of prefix
	if(x < 4) {
		AppendPart(&buf, i-NUM_POLISH_2_O, polish_2);
		return 0;
	}

	t = _polish_types;
	if(IS_INT_INSIDE(i, NUM_POLISH_2_O, NUM_POLISH_2_O + NUM_POLISH_2_M)) {
		// nothing
	} else if (IS_INT_INSIDE(i, NUM_POLISH_2_O + NUM_POLISH_2_M, NUM_POLISH_2_O + NUM_POLISH_2_M + NUM_POLISH_2_F)) {
		t += 1;
	} else {
		t += 2;
	}

	// suffix or prefix
	if(x < 8) {
		AppendPart(&buf, GETNUM(10, NUM_POLISH_1), (*t)[0]);
		AppendPart(&buf, i, polish_2);
	} else {
		AppendPart(&buf, i, polish_2);
		AppendPart(&buf, GETNUM(10, NUM_POLISH_3), (*t)[1]);
	}

	return 0;
}

#define NUM_SLOVAKISH_1 87
static const char slovakish_1[] =
MK("Bratislava")
MK("Banovce nad Bebravou")
MK("Banska Bystrica")
MK("Banska Stiavnica")
MK("Bardejov")
MK("Brezno")
MK("Brezova pod Bradlom")
MK("Bytca")
MK("Cadca")
MK("Cierna nad Tisou")
MK("Detva")
MK("Detva")
MK("Dolny Kubin")
MK("Dolny Kubin")
MK("Dunajska Streda")
MK("Gabcikovo")
MK("Galanta")
MK("Gbely")
MK("Gelnica")
MK("Handlova")
MK("Hlohovec")
MK("Holic")
MK("Humenne")
MK("Hurbanovo")
MK("Kezmarok")
MK("Komarno")
MK("Kosice")
MK("Kremnica")
MK("Krompachy")
MK("Kuty")
MK("Leopoldov")
MK("Levoca")
MK("Liptovsky Mikulas")
MK("Lucenec")
MK("Malacky")
MK("Martin")
MK("Medzilaborce")
MK("Michalovce")
MK("Modra")
MK("Myjava")
MK("Namestovo")
MK("Nitra")
MK("Nova Bana")
MK("Nove Mesto nad Vahom")
MK("Nove Zamky")
MK("Partizanske")
MK("Pezinok")
MK("Piestany")
MK("Poltar")
MK("Poprad")
MK("Povazska Bystrica")
MK("Prievidza")
MK("Puchov")
MK("Revuca")
MK("Rimavska Sobota")
MK("Roznava")
MK("Ruzomberok")
MK("Sabinov")
MK("Sala")
MK("Senec")
MK("Senica")
MK("Sered")
MK("Skalica")
MK("Sladkovicovo")
MK("Smolenice")
MK("Snina")
MK("Stara Lubovna")
MK("Stara Tura")
MK("Strazske")
MK("Stropkov")
MK("Stupava")
MK("Sturovo")
MK("Sulekovo")
MK("Topolcany")
MK("Trebisov")
MK("Trencin")
MK("Trnava")
MK("Turcianske Teplice")
MK("Tvrdosin")
MK("Vrable")
MK("Vranov nad Toplov")
MK("Zahorska Bystrica")
MK("Zdiar")
MK("Ziar nad Hronom")
MK("Zilina")
MK("Zlate Moravce")
MK("Zvolen")
;

static byte MakeSlovakishTownName(byte *buf, uint32 seed)
{
	AppendPart(&buf, GETNUM(0, NUM_SLOVAKISH_1), slovakish_1);
	return 0;	
}

// Modifiers
#define NUM_HUNGARIAN_1 5
static const char hungarian_1[] = 
	MK("Nagy-")
	MK("Kis-")
	MK("Felsõ-")
	MK("Alsó-")
	MK("Új-")
;

#define NUM_HUNGARIAN_2 54
static const char hungarian_2[] = 
// River modifiers
// 1 - 10
	MK("Bodrog")
	MK("Dráva")
	MK("Duna")
	MK("Hejõ")
	MK("Hernád")
	MK("Rába")
	MK("Sajó")
	MK("Szamos")
	MK("Tisza")
	MK("Zala")
// Lake modifiers
// 11 - 12
	MK("Balaton")
	MK("Fertõ")
// Mountain modifiers
// 13 - 14
	MK("Bakony")
	MK("Cserhát")
// Country modifiers
// 15 - 23
	MK("Bihar")
	MK("Hajdú")
	MK("Jász")
	MK("Kun")
	MK("Magyar")
	MK("Nógrád")
	MK("Nyír")
	MK("Somogy")
	MK("Székely")
// Town modifiers
// 24 - 26
	MK("Buda")
	MK("Gyõr")
	MK("Pest")
// Color modifiers
// 27
	MK("Fehér")
// General terrain modifiers
// 28 - 34
	MK("Cserép")
	MK("Erdõ")
	MK("Hegy")
	MK("Homok")
	MK("Mezõ")
	MK("Puszta")
	MK("Sár")
// Rank modifiers
// 35 - 40
	MK("Császár")
	MK("Herceg")
	MK("Király")
	MK("Nemes")
	MK("Püspök")
	MK("Szent")
// Plant modifiers
// 41 - 42
	MK("Almás")
	MK("Szilvás")
// Standard stuff
// 43 - 54
	MK("Agg")
	MK("Aranyos")
	MK("Békés")
	MK("Egyházas")
	MK("Gagy")
	MK("Heves")
        MK("Kapos")
	MK("Tápió")
	MK("Torna")
	MK("Vas")
	MK("Vámos")
	MK("Vásáros")
;

#define NUM_HUNGARIAN_3 16
static const char hungarian_3[] = 
	MK("apáti")
	MK("bába")
	MK("bikk")
	MK("dob")
	MK("fa")
	MK("föld")
	MK("hegyes")
	MK("kak")
	MK("kereszt")
	MK("kürt")
	MK("ladány")
	MK("mérges")
	MK("szalonta")
	MK("telek")
	MK("vas")
	MK("völgy")
;

#define NUM_HUNGARIAN_4 5
static const char hungarian_4[] = 
	MK("alja")
	MK("egyháza")
	MK("háza")
	MK("úr")
	MK("vár")
;

#define NUM_HUNGARIAN_REAL 35
static const char hungarian_real[] =
	MK("Ajka")
	MK("Aszód")
	MK("Badacsony")
	MK("Baja")
	MK("Budapest")
	MK("Debrecen")
	MK("Eger")
	MK("Fonyód")
	MK("Gödöllõ")
	MK("Gyõr")
	MK("Gyula")
	MK("Karcag")
	MK("Kecskemét")
	MK("Keszthely")
	MK("Kisköre")
	MK("Kocsord")
	MK("Komárom")
	MK("Kõszeg")
	MK("Makó")
	MK("Mohács")
	MK("Miskolc")
	MK("Ózd")
	MK("Paks")
	MK("Pápa")
	MK("Pécs")
	MK("Polgár")
	MK("Sarkad")
	MK("Siófok")
	MK("Szeged")
	MK("Szentes")
	MK("Szolnok")
	MK("Tihany")
	MK("Tokaj")
	MK("Vác")
	MK("Záhony")
	MK("Zirc")
;

static byte MakeHungarianTownName(byte *buf, uint32 seed)
{
	int i;

	if (GETNUM(12, 15) < 3) {
		/* These are real names.. */
		AppendPart(&buf, GETNUM(0, NUM_HUNGARIAN_REAL), hungarian_real);
	} else {
		/* These are the generated names.. Some of them exist, LOL */
		/* Append the prefix if needed */
		i = GETNUM(3, NUM_HUNGARIAN_1 * 3);
		if (i < NUM_HUNGARIAN_1) AppendPart(&buf, i, hungarian_1);

		AppendPart(&buf, GETNUM(3, NUM_HUNGARIAN_2), hungarian_2);
		AppendPart(&buf, GETNUM(6, NUM_HUNGARIAN_3), hungarian_3);
		
		i = GETNUM(10, NUM_HUNGARIAN_4 * 3);
		if (i < NUM_HUNGARIAN_4) AppendPart(&buf, i, hungarian_4);
	}
	return 0;
}

TownNameGenerator * const _town_name_generators[] = {
	MakeEnglishTownName,
	MakeFrenchTownName,
	MakeGermanTownName,
	MakeAmericanTownName,
	MakeSpanishTownName,
	MakeSillyTownName,
	MakeSwedishTownName,
	MakeDutchTownName,
	MakeFinnishTownName,
	MakePolishTownName,
	MakeSlovakishTownName,
	MakeHungarianTownName,
	MakeAustrianTownName
};

#define FIXNUM(x, y, z) (((((x) << 16) / (y)) + 1) << z)

uint32 GetOldTownName(uint32 townnameparts, byte old_town_name_type)
{
	uint32 a = 0;
	switch (old_town_name_type) {
		case 0: case 3: /* English, American */
			/*	Already OK */
			return townnameparts;
		case 1: /* French */
			/*	For some reason 86 needs to be subtracted from townnameparts
			 *	0000 0000 0000 0000 0000 0000 1111 1111 */
			return FIXNUM(townnameparts - 86, NUM_FRENCH_1, 0);
		case 2: /* German */
			#ifdef _DEBUG
				printf("German Townnames are buggy... (%d)\n", townnameparts);
			#endif
			return townnameparts;
		case 4: /* Latin-American */
			/*	0000 0000 0000 0000 0000 0000 1111 1111 */
			return FIXNUM(townnameparts, NUM_SPANISH_1, 0);
		case 5: /* Silly */
				//AppendPart(&buf, GETNUM(16, NUM_SILLY_2),silly_2);
			/*	NUM_SILLY_1	-	lower 16 bits
			 *	NUM_SILLY_2	-	upper 16 bits without leading 1 (first 8 bytes)
			 *	1000 0000 2222 2222 0000 0000 1111 1111 */
			return FIXNUM(townnameparts, NUM_SILLY_1, 0) | FIXNUM(((townnameparts >> 16)&0xFF), NUM_SILLY_2, 16);
	}
	return 0;
}
