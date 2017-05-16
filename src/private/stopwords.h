/* This file is automatically generated. DO NOT EDIT!
   Instead, edit gen-stopwords.py and re-run.  */

#ifndef STOPWORDS_H
#define STOPWORDS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct stopword_list {
	const char *name;
	int offset;
	int length;
};

static const char *stopword_list_names[] = {
	"danish",
	"dutch",
	"english",
	"finnish",
	"french",
	"german",
	"hungarian",
	"italian",
	"norwegian",
	"portuguese",
	"russian",
	"spanish",
	"swedish",
	NULL
};

static struct stopword_list stopword_lists[] = {
	{"danish", 0, 94},
	{"dutch", 95, 101},
	{"english", 197, 174},
	{"finnish", 372, 235},
	{"french", 608, 164},
	{"german", 773, 231},
	{"hungarian", 1005, 199},
	{"italian", 1205, 279},
	{"norwegian", 1485, 176},
	{"portuguese", 1662, 203},
	{"russian", 1866, 159},
	{"spanish", 2026, 308},
	{"swedish", 2335, 114},
	{NULL, 0, 0}
};

static const char * stopword_strings[] = {
	/* danish */
	"ad",
	"af",
	"alle",
	"alt",
	"anden",
	"at",
	"blev",
	"blive",
	"bliver",
	"da",
	"de",
	"dem",
	"den",
	"denne",
	"der",
	"deres",
	"det",
	"dette",
	"dig",
	"din",
	"disse",
	"dog",
	"du",
	"efter",
	"eller",
	"en",
	"end",
	"er",
	"et",
	"for",
	"fra",
	"ham",
	"han",
	"hans",
	"har",
	"havde",
	"have",
	"hende",
	"hendes",
	"her",
	"hos",
	"hun",
	"hvad",
	"hvis",
	"hvor",
	"i",
	"ikke",
	"ind",
	"jeg",
	"jer",
	"jo",
	"kunne",
	"man",
	"mange",
	"med",
	"meget",
	"men",
	"mig",
	"min",
	"mine",
	"mit",
	"mod",
	"ned",
	"noget",
	"nogle",
	"nu",
	"n\303\245r",
	"og",
	"ogs\303\245",
	"om",
	"op",
	"os",
	"over",
	"p\303\245",
	"selv",
	"sig",
	"sin",
	"sine",
	"sit",
	"skal",
	"skulle",
	"som",
	"s\303\245dan",
	"thi",
	"til",
	"ud",
	"under",
	"var",
	"vi",
	"vil",
	"ville",
	"vor",
	"v\303\246re",
	"v\303\246ret",
	NULL,

	/* dutch */
	"aan",
	"al",
	"alles",
	"als",
	"altijd",
	"andere",
	"ben",
	"bij",
	"daar",
	"dan",
	"dat",
	"de",
	"der",
	"deze",
	"die",
	"dit",
	"doch",
	"doen",
	"door",
	"dus",
	"een",
	"eens",
	"en",
	"er",
	"ge",
	"geen",
	"geweest",
	"haar",
	"had",
	"heb",
	"hebben",
	"heeft",
	"hem",
	"het",
	"hier",
	"hij",
	"hoe",
	"hun",
	"iemand",
	"iets",
	"ik",
	"in",
	"is",
	"ja",
	"je",
	"kan",
	"kon",
	"kunnen",
	"maar",
	"me",
	"meer",
	"men",
	"met",
	"mij",
	"mijn",
	"moet",
	"na",
	"naar",
	"niet",
	"niets",
	"nog",
	"nu",
	"of",
	"om",
	"omdat",
	"onder",
	"ons",
	"ook",
	"op",
	"over",
	"reeds",
	"te",
	"tegen",
	"toch",
	"toen",
	"tot",
	"u",
	"uit",
	"uw",
	"van",
	"veel",
	"voor",
	"want",
	"waren",
	"was",
	"wat",
	"werd",
	"wezen",
	"wie",
	"wil",
	"worden",
	"wordt",
	"zal",
	"ze",
	"zelf",
	"zich",
	"zij",
	"zijn",
	"zo",
	"zonder",
	"zou",
	NULL,

	/* english */
	"a",
	"about",
	"above",
	"after",
	"again",
	"against",
	"all",
	"am",
	"an",
	"and",
	"any",
	"are",
	"aren't",
	"as",
	"at",
	"be",
	"because",
	"been",
	"before",
	"being",
	"below",
	"between",
	"both",
	"but",
	"by",
	"can't",
	"cannot",
	"could",
	"couldn't",
	"did",
	"didn't",
	"do",
	"does",
	"doesn't",
	"doing",
	"don't",
	"down",
	"during",
	"each",
	"few",
	"for",
	"from",
	"further",
	"had",
	"hadn't",
	"has",
	"hasn't",
	"have",
	"haven't",
	"having",
	"he",
	"he'd",
	"he'll",
	"he's",
	"her",
	"here",
	"here's",
	"hers",
	"herself",
	"him",
	"himself",
	"his",
	"how",
	"how's",
	"i",
	"i'd",
	"i'll",
	"i'm",
	"i've",
	"if",
	"in",
	"into",
	"is",
	"isn't",
	"it",
	"it's",
	"its",
	"itself",
	"let's",
	"me",
	"more",
	"most",
	"mustn't",
	"my",
	"myself",
	"no",
	"nor",
	"not",
	"of",
	"off",
	"on",
	"once",
	"only",
	"or",
	"other",
	"ought",
	"our",
	"ours",
	"ourselves",
	"out",
	"over",
	"own",
	"same",
	"shan't",
	"she",
	"she'd",
	"she'll",
	"she's",
	"should",
	"shouldn't",
	"so",
	"some",
	"such",
	"than",
	"that",
	"that's",
	"the",
	"their",
	"theirs",
	"them",
	"themselves",
	"then",
	"there",
	"there's",
	"these",
	"they",
	"they'd",
	"they'll",
	"they're",
	"they've",
	"this",
	"those",
	"through",
	"to",
	"too",
	"under",
	"until",
	"up",
	"very",
	"was",
	"wasn't",
	"we",
	"we'd",
	"we'll",
	"we're",
	"we've",
	"were",
	"weren't",
	"what",
	"what's",
	"when",
	"when's",
	"where",
	"where's",
	"which",
	"while",
	"who",
	"who's",
	"whom",
	"why",
	"why's",
	"with",
	"won't",
	"would",
	"wouldn't",
	"you",
	"you'd",
	"you'll",
	"you're",
	"you've",
	"your",
	"yours",
	"yourself",
	"yourselves",
	NULL,

	/* finnish */
	"ei",
	"eiv\303\244t",
	"emme",
	"en",
	"et",
	"ette",
	"ett\303\244",
	"he",
	"heid\303\244n",
	"heid\303\244t",
	"heihin",
	"heille",
	"heill\303\244",
	"heilt\303\244",
	"heiss\303\244",
	"heist\303\244",
	"heit\303\244",
	"h\303\244n",
	"h\303\244neen",
	"h\303\244nelle",
	"h\303\244nell\303\244",
	"h\303\244nelt\303\244",
	"h\303\244nen",
	"h\303\244ness\303\244",
	"h\303\244nest\303\244",
	"h\303\244net",
	"h\303\244nt\303\244",
	"itse",
	"ja",
	"johon",
	"joiden",
	"joihin",
	"joiksi",
	"joilla",
	"joille",
	"joilta",
	"joina",
	"joissa",
	"joista",
	"joita",
	"joka",
	"joksi",
	"jolla",
	"jolle",
	"jolta",
	"jona",
	"jonka",
	"jos",
	"jossa",
	"josta",
	"jota",
	"jotka",
	"kanssa",
	"keiden",
	"keihin",
	"keiksi",
	"keille",
	"keill\303\244",
	"keilt\303\244",
	"kein\303\244",
	"keiss\303\244",
	"keist\303\244",
	"keit\303\244",
	"keneen",
	"keneksi",
	"kenelle",
	"kenell\303\244",
	"kenelt\303\244",
	"kenen",
	"kenen\303\244",
	"keness\303\244",
	"kenest\303\244",
	"kenet",
	"ketk\303\244",
	"ketk\303\244",
	"ket\303\244",
	"koska",
	"kuin",
	"kuka",
	"kun",
	"me",
	"meid\303\244n",
	"meid\303\244t",
	"meihin",
	"meille",
	"meill\303\244",
	"meilt\303\244",
	"meiss\303\244",
	"meist\303\244",
	"meit\303\244",
	"mihin",
	"miksi",
	"mik\303\244",
	"mille",
	"mill\303\244",
	"milt\303\244",
	"mink\303\244",
	"mink\303\244",
	"minua",
	"minulla",
	"minulle",
	"minulta",
	"minun",
	"minussa",
	"minusta",
	"minut",
	"minuun",
	"min\303\244",
	"min\303\244",
	"miss\303\244",
	"mist\303\244",
	"mitk\303\244",
	"mit\303\244",
	"mukaan",
	"mutta",
	"ne",
	"niiden",
	"niihin",
	"niiksi",
	"niille",
	"niill\303\244",
	"niilt\303\244",
	"niin",
	"niin",
	"niin\303\244",
	"niiss\303\244",
	"niist\303\244",
	"niit\303\244",
	"noiden",
	"noihin",
	"noiksi",
	"noilla",
	"noille",
	"noilta",
	"noin",
	"noina",
	"noissa",
	"noista",
	"noita",
	"nuo",
	"nyt",
	"n\303\244iden",
	"n\303\244ihin",
	"n\303\244iksi",
	"n\303\244ille",
	"n\303\244ill\303\244",
	"n\303\244ilt\303\244",
	"n\303\244in\303\244",
	"n\303\244iss\303\244",
	"n\303\244ist\303\244",
	"n\303\244it\303\244",
	"n\303\244m\303\244",
	"ole",
	"olemme",
	"olen",
	"olet",
	"olette",
	"oli",
	"olimme",
	"olin",
	"olisi",
	"olisimme",
	"olisin",
	"olisit",
	"olisitte",
	"olisivat",
	"olit",
	"olitte",
	"olivat",
	"olla",
	"olleet",
	"ollut",
	"on",
	"ovat",
	"poikki",
	"se",
	"sek\303\244",
	"sen",
	"siihen",
	"siin\303\244",
	"siit\303\244",
	"siksi",
	"sille",
	"sill\303\244",
	"sill\303\244",
	"silt\303\244",
	"sinua",
	"sinulla",
	"sinulle",
	"sinulta",
	"sinun",
	"sinussa",
	"sinusta",
	"sinut",
	"sinuun",
	"sin\303\244",
	"sin\303\244",
	"sit\303\244",
	"tai",
	"te",
	"teid\303\244n",
	"teid\303\244t",
	"teihin",
	"teille",
	"teill\303\244",
	"teilt\303\244",
	"teiss\303\244",
	"teist\303\244",
	"teit\303\244",
	"tuo",
	"tuohon",
	"tuoksi",
	"tuolla",
	"tuolle",
	"tuolta",
	"tuon",
	"tuona",
	"tuossa",
	"tuosta",
	"tuota",
	"t\303\244h\303\244n",
	"t\303\244ksi",
	"t\303\244lle",
	"t\303\244ll\303\244",
	"t\303\244lt\303\244",
	"t\303\244m\303\244",
	"t\303\244m\303\244n",
	"t\303\244n\303\244",
	"t\303\244ss\303\244",
	"t\303\244st\303\244",
	"t\303\244t\303\244",
	"vaan",
	"vai",
	"vaikka",
	"yli",
	NULL,

	/* french */
	"ai",
	"aie",
	"aient",
	"aies",
	"ait",
	"as",
	"au",
	"aura",
	"aurai",
	"auraient",
	"aurais",
	"aurait",
	"auras",
	"aurez",
	"auriez",
	"aurions",
	"aurons",
	"auront",
	"aux",
	"avaient",
	"avais",
	"avait",
	"avec",
	"avez",
	"aviez",
	"avions",
	"avons",
	"ayant",
	"ayez",
	"ayons",
	"c",
	"ce",
	"ceci",
	"cela",
	"cel\303\240",
	"ces",
	"cet",
	"cette",
	"d",
	"dans",
	"de",
	"des",
	"du",
	"elle",
	"en",
	"es",
	"est",
	"et",
	"eu",
	"eue",
	"eues",
	"eurent",
	"eus",
	"eusse",
	"eussent",
	"eusses",
	"eussiez",
	"eussions",
	"eut",
	"eux",
	"e\303\273mes",
	"e\303\273t",
	"e\303\273tes",
	"furent",
	"fus",
	"fusse",
	"fussent",
	"fusses",
	"fussiez",
	"fussions",
	"fut",
	"f\303\273mes",
	"f\303\273t",
	"f\303\273tes",
	"ici",
	"il",
	"ils",
	"j",
	"je",
	"l",
	"la",
	"le",
	"les",
	"leur",
	"leurs",
	"lui",
	"m",
	"ma",
	"mais",
	"me",
	"mes",
	"moi",
	"mon",
	"m\303\252me",
	"n",
	"ne",
	"nos",
	"notre",
	"nous",
	"on",
	"ont",
	"ou",
	"par",
	"pas",
	"pour",
	"qu",
	"que",
	"quel",
	"quelle",
	"quelles",
	"quels",
	"qui",
	"s",
	"sa",
	"sans",
	"se",
	"sera",
	"serai",
	"seraient",
	"serais",
	"serait",
	"seras",
	"serez",
	"seriez",
	"serions",
	"serons",
	"seront",
	"ses",
	"soi",
	"soient",
	"sois",
	"soit",
	"sommes",
	"son",
	"sont",
	"soyez",
	"soyons",
	"suis",
	"sur",
	"t",
	"ta",
	"te",
	"tes",
	"toi",
	"ton",
	"tu",
	"un",
	"une",
	"vos",
	"votre",
	"vous",
	"y",
	"\303\240",
	"\303\251taient",
	"\303\251tais",
	"\303\251tait",
	"\303\251tant",
	"\303\251tiez",
	"\303\251tions",
	"\303\251t\303\251",
	"\303\251t\303\251e",
	"\303\251t\303\251es",
	"\303\251t\303\251s",
	"\303\252tes",
	NULL,

	/* german */
	"aber",
	"alle",
	"allem",
	"allen",
	"aller",
	"alles",
	"als",
	"also",
	"am",
	"an",
	"ander",
	"andere",
	"anderem",
	"anderen",
	"anderer",
	"anderes",
	"anderm",
	"andern",
	"anderr",
	"anders",
	"auch",
	"auf",
	"aus",
	"bei",
	"bin",
	"bis",
	"bist",
	"da",
	"damit",
	"dann",
	"das",
	"dasselbe",
	"dazu",
	"da\303\237",
	"dein",
	"deine",
	"deinem",
	"deinen",
	"deiner",
	"deines",
	"dem",
	"demselben",
	"den",
	"denn",
	"denselben",
	"der",
	"derer",
	"derselbe",
	"derselben",
	"des",
	"desselben",
	"dessen",
	"dich",
	"die",
	"dies",
	"diese",
	"dieselbe",
	"dieselben",
	"diesem",
	"diesen",
	"dieser",
	"dieses",
	"dir",
	"doch",
	"dort",
	"du",
	"durch",
	"ein",
	"eine",
	"einem",
	"einen",
	"einer",
	"eines",
	"einig",
	"einige",
	"einigem",
	"einigen",
	"einiger",
	"einiges",
	"einmal",
	"er",
	"es",
	"etwas",
	"euch",
	"euer",
	"eure",
	"eurem",
	"euren",
	"eurer",
	"eures",
	"f\303\274r",
	"gegen",
	"gewesen",
	"hab",
	"habe",
	"haben",
	"hat",
	"hatte",
	"hatten",
	"hier",
	"hin",
	"hinter",
	"ich",
	"ihm",
	"ihn",
	"ihnen",
	"ihr",
	"ihre",
	"ihrem",
	"ihren",
	"ihrer",
	"ihres",
	"im",
	"in",
	"indem",
	"ins",
	"ist",
	"jede",
	"jedem",
	"jeden",
	"jeder",
	"jedes",
	"jene",
	"jenem",
	"jenen",
	"jener",
	"jenes",
	"jetzt",
	"kann",
	"kein",
	"keine",
	"keinem",
	"keinen",
	"keiner",
	"keines",
	"k\303\266nnen",
	"k\303\266nnte",
	"machen",
	"man",
	"manche",
	"manchem",
	"manchen",
	"mancher",
	"manches",
	"mein",
	"meine",
	"meinem",
	"meinen",
	"meiner",
	"meines",
	"mich",
	"mir",
	"mit",
	"muss",
	"musste",
	"nach",
	"nicht",
	"nichts",
	"noch",
	"nun",
	"nur",
	"ob",
	"oder",
	"ohne",
	"sehr",
	"sein",
	"seine",
	"seinem",
	"seinen",
	"seiner",
	"seines",
	"selbst",
	"sich",
	"sie",
	"sind",
	"so",
	"solche",
	"solchem",
	"solchen",
	"solcher",
	"solches",
	"soll",
	"sollte",
	"sondern",
	"sonst",
	"um",
	"und",
	"uns",
	"unse",
	"unsem",
	"unsen",
	"unser",
	"unses",
	"unter",
	"viel",
	"vom",
	"von",
	"vor",
	"war",
	"waren",
	"warst",
	"was",
	"weg",
	"weil",
	"weiter",
	"welche",
	"welchem",
	"welchen",
	"welcher",
	"welches",
	"wenn",
	"werde",
	"werden",
	"wie",
	"wieder",
	"will",
	"wir",
	"wird",
	"wirst",
	"wo",
	"wollen",
	"wollte",
	"w\303\244hrend",
	"w\303\274rde",
	"w\303\274rden",
	"zu",
	"zum",
	"zur",
	"zwar",
	"zwischen",
	"\303\274ber",
	NULL,

	/* hungarian */
	"a",
	"abban",
	"ahhoz",
	"ahogy",
	"ahol",
	"aki",
	"akik",
	"akkor",
	"alatt",
	"amely",
	"amelyek",
	"amelyekben",
	"amelyeket",
	"amelyet",
	"amelynek",
	"ami",
	"amikor",
	"amit",
	"amolyan",
	"am\303\255g",
	"annak",
	"arra",
	"arr\303\263l",
	"az",
	"azok",
	"azon",
	"azonban",
	"azt",
	"azt\303\241n",
	"azut\303\241n",
	"azzal",
	"az\303\251rt",
	"be",
	"bel\303\274l",
	"benne",
	"b\303\241r",
	"cikk",
	"cikkek",
	"cikkeket",
	"csak",
	"de",
	"e",
	"ebben",
	"eddig",
	"egy",
	"egyes",
	"egyetlen",
	"egyik",
	"egyre",
	"egy\303\251b",
	"eg\303\251sz",
	"ehhez",
	"ekkor",
	"el",
	"ellen",
	"els\303\265",
	"el\303\251g",
	"el\303\265",
	"el\303\265sz\303\266r",
	"el\303\265tt",
	"emilyen",
	"ennek",
	"erre",
	"ez",
	"ezek",
	"ezen",
	"ezt",
	"ezzel",
	"ez\303\251rt",
	"fel",
	"fel\303\251",
	"hanem",
	"hiszen",
	"hogy",
	"hogyan",
	"igen",
	"ill",
	"ill.",
	"illetve",
	"ilyen",
	"ilyenkor",
	"ism\303\251t",
	"ison",
	"itt",
	"jobban",
	"j\303\263",
	"j\303\263l",
	"kell",
	"kellett",
	"keress\303\274nk",
	"kereszt\303\274l",
	"ki",
	"k\303\255v\303\274l",
	"k\303\266z\303\266tt",
	"k\303\266z\303\274l",
	"legal\303\241bb",
	"legyen",
	"lehet",
	"lehetett",
	"lenne",
	"lenni",
	"lesz",
	"lett",
	"maga",
	"mag\303\241t",
	"majd",
	"majd",
	"meg",
	"mellett",
	"mely",
	"melyek",
	"mert",
	"mi",
	"mikor",
	"milyen",
	"minden",
	"mindenki",
	"mindent",
	"mindig",
	"mint",
	"mintha",
	"mit",
	"mivel",
	"mi\303\251rt",
	"most",
	"m\303\241r",
	"m\303\241s",
	"m\303\241sik",
	"m\303\251g",
	"m\303\255g",
	"nagy",
	"nagyobb",
	"nagyon",
	"ne",
	"nekem",
	"neki",
	"nem",
	"nincs",
	"n\303\251ha",
	"n\303\251h\303\241ny",
	"n\303\251lk\303\274l",
	"olyan",
	"ott",
	"pedig",
	"persze",
	"r\303\241",
	"s",
	"saj\303\241t",
	"sem",
	"semmi",
	"sok",
	"sokat",
	"sokkal",
	"szemben",
	"szerint",
	"szinte",
	"sz\303\241m\303\241ra",
	"tal\303\241n",
	"teh\303\241t",
	"teljes",
	"tov\303\241bb",
	"tov\303\241bb\303\241",
	"t\303\266bb",
	"ugyanis",
	"utols\303\263",
	"ut\303\241n",
	"ut\303\241na",
	"vagy",
	"vagyis",
	"vagyok",
	"valaki",
	"valami",
	"valamint",
	"val\303\263",
	"van",
	"vannak",
	"vele",
	"vissza",
	"viszont",
	"volna",
	"volt",
	"voltak",
	"voltam",
	"voltunk",
	"\303\241ltal",
	"\303\241ltal\303\241ban",
	"\303\241t",
	"\303\251n",
	"\303\251ppen",
	"\303\251s",
	"\303\255gy",
	"\303\265",
	"\303\265k",
	"\303\265ket",
	"\303\266ssze",
	"\303\272gy",
	"\303\272j",
	"\303\272jabb",
	"\303\272jra",
	NULL,

	/* italian */
	"a",
	"abbia",
	"abbiamo",
	"abbiano",
	"abbiate",
	"ad",
	"agl",
	"agli",
	"ai",
	"al",
	"all",
	"alla",
	"alle",
	"allo",
	"anche",
	"avemmo",
	"avendo",
	"avesse",
	"avessero",
	"avessi",
	"avessimo",
	"aveste",
	"avesti",
	"avete",
	"aveva",
	"avevamo",
	"avevano",
	"avevate",
	"avevi",
	"avevo",
	"avrai",
	"avranno",
	"avrebbe",
	"avrebbero",
	"avrei",
	"avremmo",
	"avremo",
	"avreste",
	"avresti",
	"avrete",
	"avr\303\240",
	"avr\303\262",
	"avuta",
	"avute",
	"avuti",
	"avuto",
	"c",
	"che",
	"chi",
	"ci",
	"coi",
	"col",
	"come",
	"con",
	"contro",
	"cui",
	"da",
	"dagl",
	"dagli",
	"dai",
	"dal",
	"dall",
	"dalla",
	"dalle",
	"dallo",
	"degl",
	"degli",
	"dei",
	"del",
	"dell",
	"della",
	"delle",
	"dello",
	"di",
	"dov",
	"dove",
	"e",
	"ebbe",
	"ebbero",
	"ebbi",
	"ed",
	"era",
	"erano",
	"eravamo",
	"eravate",
	"eri",
	"ero",
	"essendo",
	"faccia",
	"facciamo",
	"facciano",
	"facciate",
	"faccio",
	"facemmo",
	"facendo",
	"facesse",
	"facessero",
	"facessi",
	"facessimo",
	"faceste",
	"facesti",
	"faceva",
	"facevamo",
	"facevano",
	"facevate",
	"facevi",
	"facevo",
	"fai",
	"fanno",
	"farai",
	"faranno",
	"farebbe",
	"farebbero",
	"farei",
	"faremmo",
	"faremo",
	"fareste",
	"faresti",
	"farete",
	"far\303\240",
	"far\303\262",
	"fece",
	"fecero",
	"feci",
	"fosse",
	"fossero",
	"fossi",
	"fossimo",
	"foste",
	"fosti",
	"fu",
	"fui",
	"fummo",
	"furono",
	"gli",
	"ha",
	"hai",
	"hanno",
	"ho",
	"i",
	"il",
	"in",
	"io",
	"l",
	"la",
	"le",
	"lei",
	"li",
	"lo",
	"loro",
	"lui",
	"ma",
	"mi",
	"mia",
	"mie",
	"miei",
	"mio",
	"ne",
	"negl",
	"negli",
	"nei",
	"nel",
	"nell",
	"nella",
	"nelle",
	"nello",
	"noi",
	"non",
	"nostra",
	"nostre",
	"nostri",
	"nostro",
	"o",
	"per",
	"perch\303\251",
	"pi\303\271",
	"quale",
	"quanta",
	"quante",
	"quanti",
	"quanto",
	"quella",
	"quelle",
	"quelli",
	"quello",
	"questa",
	"queste",
	"questi",
	"questo",
	"sarai",
	"saranno",
	"sarebbe",
	"sarebbero",
	"sarei",
	"saremmo",
	"saremo",
	"sareste",
	"saresti",
	"sarete",
	"sar\303\240",
	"sar\303\262",
	"se",
	"sei",
	"si",
	"sia",
	"siamo",
	"siano",
	"siate",
	"siete",
	"sono",
	"sta",
	"stai",
	"stando",
	"stanno",
	"starai",
	"staranno",
	"starebbe",
	"starebbero",
	"starei",
	"staremmo",
	"staremo",
	"stareste",
	"staresti",
	"starete",
	"star\303\240",
	"star\303\262",
	"stava",
	"stavamo",
	"stavano",
	"stavate",
	"stavi",
	"stavo",
	"stemmo",
	"stesse",
	"stessero",
	"stessi",
	"stessimo",
	"steste",
	"stesti",
	"stette",
	"stettero",
	"stetti",
	"stia",
	"stiamo",
	"stiano",
	"stiate",
	"sto",
	"su",
	"sua",
	"sue",
	"sugl",
	"sugli",
	"sui",
	"sul",
	"sull",
	"sulla",
	"sulle",
	"sullo",
	"suo",
	"suoi",
	"ti",
	"tra",
	"tu",
	"tua",
	"tue",
	"tuo",
	"tuoi",
	"tutti",
	"tutto",
	"un",
	"una",
	"uno",
	"vi",
	"voi",
	"vostra",
	"vostre",
	"vostri",
	"vostro",
	"\303\250",
	NULL,

	/* norwegian */
	"alle",
	"at",
	"av",
	"bare",
	"begge",
	"ble",
	"blei",
	"bli",
	"blir",
	"blitt",
	"b\303\245de",
	"b\303\245e",
	"da",
	"de",
	"deg",
	"dei",
	"deim",
	"deira",
	"deires",
	"dem",
	"den",
	"denne",
	"der",
	"dere",
	"deres",
	"det",
	"dette",
	"di",
	"din",
	"disse",
	"ditt",
	"du",
	"dykk",
	"dykkar",
	"d\303\245",
	"eg",
	"ein",
	"eit",
	"eitt",
	"eller",
	"elles",
	"en",
	"enn",
	"er",
	"et",
	"ett",
	"etter",
	"for",
	"fordi",
	"fra",
	"f\303\270r",
	"ha",
	"hadde",
	"han",
	"hans",
	"har",
	"hennar",
	"henne",
	"hennes",
	"her",
	"hj\303\245",
	"ho",
	"hoe",
	"honom",
	"hoss",
	"hossen",
	"hun",
	"hva",
	"hvem",
	"hver",
	"hvilke",
	"hvilken",
	"hvis",
	"hvor",
	"hvordan",
	"hvorfor",
	"i",
	"ikke",
	"ikkje",
	"ikkje",
	"ingen",
	"ingi",
	"inkje",
	"inn",
	"inni",
	"ja",
	"jeg",
	"kan",
	"kom",
	"korleis",
	"korso",
	"kun",
	"kunne",
	"kva",
	"kvar",
	"kvarhelst",
	"kven",
	"kvi",
	"kvifor",
	"man",
	"mange",
	"me",
	"med",
	"medan",
	"meg",
	"meget",
	"mellom",
	"men",
	"mi",
	"min",
	"mine",
	"mitt",
	"mot",
	"mykje",
	"ned",
	"no",
	"noe",
	"noen",
	"noka",
	"noko",
	"nokon",
	"nokor",
	"nokre",
	"n\303\245",
	"n\303\245r",
	"og",
	"ogs\303\245",
	"om",
	"opp",
	"oss",
	"over",
	"p\303\245",
	"samme",
	"seg",
	"selv",
	"si",
	"si",
	"sia",
	"sidan",
	"siden",
	"sin",
	"sine",
	"sitt",
	"sj\303\270l",
	"skal",
	"skulle",
	"slik",
	"so",
	"som",
	"som",
	"somme",
	"somt",
	"s\303\245",
	"s\303\245nn",
	"til",
	"um",
	"upp",
	"ut",
	"uten",
	"var",
	"vart",
	"varte",
	"ved",
	"vere",
	"verte",
	"vi",
	"vil",
	"ville",
	"vore",
	"vors",
	"vort",
	"v\303\245r",
	"v\303\246re",
	"v\303\246re",
	"v\303\246rt",
	"\303\245",
	NULL,

	/* portuguese */
	"a",
	"ao",
	"aos",
	"aquela",
	"aquelas",
	"aquele",
	"aqueles",
	"aquilo",
	"as",
	"at\303\251",
	"com",
	"como",
	"da",
	"das",
	"de",
	"dela",
	"delas",
	"dele",
	"deles",
	"depois",
	"do",
	"dos",
	"e",
	"ela",
	"elas",
	"ele",
	"eles",
	"em",
	"entre",
	"era",
	"eram",
	"essa",
	"essas",
	"esse",
	"esses",
	"esta",
	"estamos",
	"estas",
	"estava",
	"estavam",
	"este",
	"esteja",
	"estejam",
	"estejamos",
	"estes",
	"esteve",
	"estive",
	"estivemos",
	"estiver",
	"estivera",
	"estiveram",
	"estiverem",
	"estivermos",
	"estivesse",
	"estivessem",
	"estiv\303\251ramos",
	"estiv\303\251ssemos",
	"estou",
	"est\303\241",
	"est\303\241vamos",
	"est\303\243o",
	"eu",
	"foi",
	"fomos",
	"for",
	"fora",
	"foram",
	"forem",
	"formos",
	"fosse",
	"fossem",
	"fui",
	"f\303\264ramos",
	"f\303\264ssemos",
	"haja",
	"hajam",
	"hajamos",
	"havemos",
	"hei",
	"houve",
	"houvemos",
	"houver",
	"houvera",
	"houveram",
	"houverei",
	"houverem",
	"houveremos",
	"houveria",
	"houveriam",
	"houvermos",
	"houver\303\241",
	"houver\303\243o",
	"houver\303\255amos",
	"houvesse",
	"houvessem",
	"houv\303\251ramos",
	"houv\303\251ssemos",
	"h\303\241",
	"h\303\243o",
	"isso",
	"isto",
	"j\303\241",
	"lhe",
	"lhes",
	"mais",
	"mas",
	"me",
	"mesmo",
	"meu",
	"meus",
	"minha",
	"minhas",
	"muito",
	"na",
	"nas",
	"nem",
	"no",
	"nos",
	"nossa",
	"nossas",
	"nosso",
	"nossos",
	"num",
	"numa",
	"n\303\243o",
	"n\303\263s",
	"o",
	"os",
	"ou",
	"para",
	"pela",
	"pelas",
	"pelo",
	"pelos",
	"por",
	"qual",
	"quando",
	"que",
	"quem",
	"se",
	"seja",
	"sejam",
	"sejamos",
	"sem",
	"serei",
	"seremos",
	"seria",
	"seriam",
	"ser\303\241",
	"ser\303\243o",
	"ser\303\255amos",
	"seu",
	"seus",
	"somos",
	"sou",
	"sua",
	"suas",
	"s\303\243o",
	"s\303\263",
	"tamb\303\251m",
	"te",
	"tem",
	"temos",
	"tenha",
	"tenham",
	"tenhamos",
	"tenho",
	"terei",
	"teremos",
	"teria",
	"teriam",
	"ter\303\241",
	"ter\303\243o",
	"ter\303\255amos",
	"teu",
	"teus",
	"teve",
	"tinha",
	"tinham",
	"tive",
	"tivemos",
	"tiver",
	"tivera",
	"tiveram",
	"tiverem",
	"tivermos",
	"tivesse",
	"tivessem",
	"tiv\303\251ramos",
	"tiv\303\251ssemos",
	"tu",
	"tua",
	"tuas",
	"t\303\251m",
	"t\303\255nhamos",
	"um",
	"uma",
	"voc\303\252",
	"voc\303\252s",
	"vos",
	"\303\240",
	"\303\240s",
	"\303\251ramos",
	NULL,

	/* russian */
	"\320\221",
	"\320\222\320\225\320\252",
	"\320\222\320\237\320\234\320\225\320\225",
	"\320\222\320\237\320\234\320\250\320\253\320\225",
	"\320\222\320\245\320\224\320\225\320\244",
	"\320\222\320\245\320\224\320\244\320\237",
	"\320\222\320\251",
	"\320\222\320\251\320\234",
	"\320\222\320\251\320\234\320\221",
	"\320\222\320\251\320\234\320\231",
	"\320\222\320\251\320\234\320\237",
	"\320\222\320\251\320\244\320\250",
	"\320\224\320\221",
	"\320\224\320\221\320\246\320\225",
	"\320\224\320\234\320\241",
	"\320\224\320\237",
	"\320\224\320\242\320\245\320\227\320\237\320\232",
	"\320\224\320\247\320\221",
	"\320\225\320\225",
	"\320\225\320\227\320\237",
	"\320\225\320\232",
	"\320\225\320\235\320\245",
	"\320\225\320\243\320\234\320\231",
	"\320\225\320\243\320\244\320\250",
	"\320\225\320\255\320\225",
	"\320\227\320\224\320\225",
	"\320\227\320\237\320\247\320\237\320\242\320\231\320\234",
	"\320\230\320\237\320\242\320\237\320\253\320\237",
	"\320\230\320\237\320\244\320\250",
	"\320\231",
	"\320\231\320\230",
	"\320\231\320\234\320\231",
	"\320\231\320\235",
	"\320\231\320\236\320\237\320\227\320\224\320\221",
	"\320\231\320\252",
	"\320\233",
	"\320\233\320\221\320\233",
	"\320\233\320\221\320\233\320\221\320\241",
	"\320\233\320\221\320\233\320\237\320\232",
	"\320\233\320\221\320\246\320\225\320\244\320\243\320\241",
	"\320\233\320\237\320\227\320\224\320\221",
	"\320\233\320\237\320\236\320\225\320\256\320\236\320\237",
	"\320\233\320\244\320\237",
	"\320\233\320\245\320\224\320\221",
	"\320\234\320\231",
	"\320\234\320\245\320\256\320\253\320\225",
	"\320\235\320\225\320\236\320\241",
	"\320\235\320\225\320\246\320\224\320\245",
	"\320\235\320\236\320\225",
	"\320\235\320\236\320\237\320\227\320\237",
	"\320\235\320\237\320\232",
	"\320\235\320\237\320\241",
	"\320\235\320\237\320\246\320\225\320\244",
	"\320\235\320\237\320\246\320\236\320\237",
	"\320\235\320\251",
	"\320\236\320\221",
	"\320\236\320\221\320\224",
	"\320\236\320\221\320\224\320\237",
	"\320\236\320\221\320\233\320\237\320\236\320\225\320\223",
	"\320\236\320\221\320\243",
	"\320\236\320\225",
	"\320\236\320\225\320\225",
	"\320\236\320\225\320\227\320\237",
	"\320\236\320\225\320\232",
	"\320\236\320\225\320\234\320\250\320\252\320\241",
	"\320\236\320\225\320\244",
	"\320\236\320\231",
	"\320\236\320\231\320\222\320\245\320\224\320\250",
	"\320\236\320\231\320\230",
	"\320\236\320\231\320\233\320\237\320\227\320\224\320\221",
	"\320\236\320\231\320\235",
	"\320\236\320\231\320\256\320\225\320\227\320\237",
	"\320\236\320\237",
	"\320\236\320\245",
	"\320\237",
	"\320\237\320\222",
	"\320\237\320\224\320\231\320\236",
	"\320\237\320\236",
	"\320\237\320\236\320\221",
	"\320\237\320\236\320\231",
	"\320\237\320\240\320\241\320\244\320\250",
	"\320\237\320\244",
	"\320\240\320\225\320\242\320\225\320\224",
	"\320\240\320\237",
	"\320\240\320\237\320\224",
	"\320\240\320\237\320\243\320\234\320\225",
	"\320\240\320\237\320\244\320\237\320\235",
	"\320\240\320\237\320\244\320\237\320\235\320\245",
	"\320\240\320\237\320\256\320\244\320\231",
	"\320\240\320\242\320\231",
	"\320\240\320\242\320\237",
	"\320\241",
	"\320\242\320\221\320\252",
	"\320\242\320\221\320\252\320\247\320\225",
	"\320\243",
	"\320\243\320\221\320\235",
	"\320\243\320\225\320\222\320\225",
	"\320\243\320\225\320\222\320\241",
	"\320\243\320\225\320\227\320\237\320\224\320\236\320\241",
	"\320\243\320\225\320\232\320\256\320\221\320\243",
	"\320\243\320\233\320\221\320\252\320\221\320\234",
	"\320\243\320\233\320\221\320\252\320\221\320\234\320\221",
	"\320\243\320\233\320\221\320\252\320\221\320\244\320\250",
	"\320\243\320\237",
	"\320\243\320\237\320\247\320\243\320\225\320\235",
	"\320\243\320\247\320\237\320\220",
	"\320\244\320\221\320\233",
	"\320\244\320\221\320\233\320\237\320\232",
	"\320\244\320\221\320\235",
	"\320\244\320\225\320\222\320\241",
	"\320\244\320\225\320\235",
	"\320\244\320\225\320\240\320\225\320\242\320\250",
	"\320\244\320\237",
	"\320\244\320\237\320\227\320\224\320\221",
	"\320\244\320\237\320\227\320\237",
	"\320\244\320\237\320\234\320\250\320\233\320\237",
	"\320\244\320\237\320\235",
	"\320\244\320\237\320\244",
	"\320\244\320\237\320\246\320\225",
	"\320\244\320\242\320\231",
	"\320\244\320\245\320\244",
	"\320\244\320\251",
	"\320\245",
	"\320\245\320\246",
	"\320\245\320\246\320\225",
	"\320\246",
	"\320\246\320\225",
	"\320\246\320\231\320\252\320\236\320\250",
	"\320\247",
	"\320\247\320\221\320\235",
	"\320\247\320\221\320\243",
	"\320\247\320\224\320\242\320\245\320\227",
	"\320\247\320\225\320\224\320\250",
	"\320\247\320\237",
	"\320\247\320\237\320\244",
	"\320\247\320\240\320\242\320\237\320\256\320\225\320\235",
	"\320\247\320\243\320\220",
	"\320\247\320\243\320\225",
	"\320\247\320\243\320\225\320\227\320\224\320\221",
	"\320\247\320\243\320\225\320\227\320\237",
	"\320\247\320\243\320\225\320\230",
	"\320\247\320\251",
	"\320\252\320\221",
	"\320\252\320\221\320\256\320\225\320\235",
	"\320\252\320\224\320\225\320\243\320\250",
	"\320\254\320\244\320\231",
	"\320\254\320\244\320\237\320\227\320\237",
	"\320\254\320\244\320\237\320\232",
	"\320\254\320\244\320\237\320\235",
	"\320\254\320\244\320\237\320\244",
	"\320\254\320\244\320\245",
	"\320\256\320\225\320\227\320\237",
	"\320\256\320\225\320\234\320\237\320\247\320\225\320\233",
	"\320\256\320\225\320\235",
	"\320\256\320\225\320\242\320\225\320\252",
	"\320\256\320\244\320\237",
	"\320\256\320\244\320\237\320\222",
	"\320\256\320\244\320\237\320\222\320\251",
	"\320\256\320\245\320\244\320\250",
	NULL,

	/* spanish */
	"a",
	"al",
	"algo",
	"algunas",
	"algunos",
	"ante",
	"antes",
	"como",
	"con",
	"contra",
	"cual",
	"cuando",
	"de",
	"del",
	"desde",
	"donde",
	"durante",
	"e",
	"el",
	"ella",
	"ellas",
	"ellos",
	"en",
	"entre",
	"era",
	"erais",
	"eran",
	"eras",
	"eres",
	"es",
	"esa",
	"esas",
	"ese",
	"eso",
	"esos",
	"esta",
	"estaba",
	"estabais",
	"estaban",
	"estabas",
	"estad",
	"estada",
	"estadas",
	"estado",
	"estados",
	"estamos",
	"estando",
	"estar",
	"estaremos",
	"estar\303\241",
	"estar\303\241n",
	"estar\303\241s",
	"estar\303\251",
	"estar\303\251is",
	"estar\303\255a",
	"estar\303\255ais",
	"estar\303\255amos",
	"estar\303\255an",
	"estar\303\255as",
	"estas",
	"este",
	"estemos",
	"esto",
	"estos",
	"estoy",
	"estuve",
	"estuviera",
	"estuvierais",
	"estuvieran",
	"estuvieras",
	"estuvieron",
	"estuviese",
	"estuvieseis",
	"estuviesen",
	"estuvieses",
	"estuvimos",
	"estuviste",
	"estuvisteis",
	"estuvi\303\251ramos",
	"estuvi\303\251semos",
	"estuvo",
	"est\303\241",
	"est\303\241bamos",
	"est\303\241is",
	"est\303\241n",
	"est\303\241s",
	"est\303\251",
	"est\303\251is",
	"est\303\251n",
	"est\303\251s",
	"fue",
	"fuera",
	"fuerais",
	"fueran",
	"fueras",
	"fueron",
	"fuese",
	"fueseis",
	"fuesen",
	"fueses",
	"fui",
	"fuimos",
	"fuiste",
	"fuisteis",
	"fu\303\251ramos",
	"fu\303\251semos",
	"ha",
	"habida",
	"habidas",
	"habido",
	"habidos",
	"habiendo",
	"habremos",
	"habr\303\241",
	"habr\303\241n",
	"habr\303\241s",
	"habr\303\251",
	"habr\303\251is",
	"habr\303\255a",
	"habr\303\255ais",
	"habr\303\255amos",
	"habr\303\255an",
	"habr\303\255as",
	"hab\303\251is",
	"hab\303\255a",
	"hab\303\255ais",
	"hab\303\255amos",
	"hab\303\255an",
	"hab\303\255as",
	"han",
	"has",
	"hasta",
	"hay",
	"haya",
	"hayamos",
	"hayan",
	"hayas",
	"hay\303\241is",
	"he",
	"hemos",
	"hube",
	"hubiera",
	"hubierais",
	"hubieran",
	"hubieras",
	"hubieron",
	"hubiese",
	"hubieseis",
	"hubiesen",
	"hubieses",
	"hubimos",
	"hubiste",
	"hubisteis",
	"hubi\303\251ramos",
	"hubi\303\251semos",
	"hubo",
	"la",
	"las",
	"le",
	"les",
	"lo",
	"los",
	"me",
	"mi",
	"mis",
	"mucho",
	"muchos",
	"muy",
	"m\303\241s",
	"m\303\255",
	"m\303\255a",
	"m\303\255as",
	"m\303\255o",
	"m\303\255os",
	"nada",
	"ni",
	"no",
	"nos",
	"nosotras",
	"nosotros",
	"nuestra",
	"nuestras",
	"nuestro",
	"nuestros",
	"o",
	"os",
	"otra",
	"otras",
	"otro",
	"otros",
	"para",
	"pero",
	"poco",
	"por",
	"porque",
	"que",
	"quien",
	"quienes",
	"qu\303\251",
	"se",
	"sea",
	"seamos",
	"sean",
	"seas",
	"seremos",
	"ser\303\241",
	"ser\303\241n",
	"ser\303\241s",
	"ser\303\251",
	"ser\303\251is",
	"ser\303\255a",
	"ser\303\255ais",
	"ser\303\255amos",
	"ser\303\255an",
	"ser\303\255as",
	"se\303\241is",
	"sido",
	"siendo",
	"sin",
	"sobre",
	"sois",
	"somos",
	"son",
	"soy",
	"su",
	"sus",
	"suya",
	"suyas",
	"suyo",
	"suyos",
	"s\303\255",
	"tambi\303\251n",
	"tanto",
	"te",
	"tendremos",
	"tendr\303\241",
	"tendr\303\241n",
	"tendr\303\241s",
	"tendr\303\251",
	"tendr\303\251is",
	"tendr\303\255a",
	"tendr\303\255ais",
	"tendr\303\255amos",
	"tendr\303\255an",
	"tendr\303\255as",
	"tened",
	"tenemos",
	"tenga",
	"tengamos",
	"tengan",
	"tengas",
	"tengo",
	"teng\303\241is",
	"tenida",
	"tenidas",
	"tenido",
	"tenidos",
	"teniendo",
	"ten\303\251is",
	"ten\303\255a",
	"ten\303\255ais",
	"ten\303\255amos",
	"ten\303\255an",
	"ten\303\255as",
	"ti",
	"tiene",
	"tienen",
	"tienes",
	"todo",
	"todos",
	"tu",
	"tus",
	"tuve",
	"tuviera",
	"tuvierais",
	"tuvieran",
	"tuvieras",
	"tuvieron",
	"tuviese",
	"tuvieseis",
	"tuviesen",
	"tuvieses",
	"tuvimos",
	"tuviste",
	"tuvisteis",
	"tuvi\303\251ramos",
	"tuvi\303\251semos",
	"tuvo",
	"tuya",
	"tuyas",
	"tuyo",
	"tuyos",
	"t\303\272",
	"un",
	"una",
	"uno",
	"unos",
	"vosotras",
	"vosotros",
	"vuestra",
	"vuestras",
	"vuestro",
	"vuestros",
	"y",
	"ya",
	"yo",
	"\303\251l",
	"\303\251ramos",
	NULL,

	/* swedish */
	"alla",
	"allt",
	"att",
	"av",
	"blev",
	"bli",
	"blir",
	"blivit",
	"de",
	"dem",
	"den",
	"denna",
	"deras",
	"dess",
	"dessa",
	"det",
	"detta",
	"dig",
	"din",
	"dina",
	"ditt",
	"du",
	"d\303\244r",
	"d\303\245",
	"efter",
	"ej",
	"eller",
	"en",
	"er",
	"era",
	"ert",
	"ett",
	"fr\303\245n",
	"f\303\266r",
	"ha",
	"hade",
	"han",
	"hans",
	"har",
	"henne",
	"hennes",
	"hon",
	"honom",
	"hur",
	"h\303\244r",
	"i",
	"icke",
	"ingen",
	"inom",
	"inte",
	"jag",
	"ju",
	"kan",
	"kunde",
	"man",
	"med",
	"mellan",
	"men",
	"mig",
	"min",
	"mina",
	"mitt",
	"mot",
	"mycket",
	"ni",
	"nu",
	"n\303\244r",
	"n\303\245gon",
	"n\303\245got",
	"n\303\245gra",
	"och",
	"om",
	"oss",
	"p\303\245",
	"samma",
	"sedan",
	"sig",
	"sin",
	"sina",
	"sitta",
	"sj\303\244lv",
	"skulle",
	"som",
	"s\303\245",
	"s\303\245dan",
	"s\303\245dana",
	"s\303\245dant",
	"till",
	"under",
	"upp",
	"ut",
	"utan",
	"vad",
	"var",
	"vara",
	"varf\303\266r",
	"varit",
	"varje",
	"vars",
	"vart",
	"vem",
	"vi",
	"vid",
	"vilka",
	"vilkas",
	"vilken",
	"vilket",
	"v\303\245r",
	"v\303\245ra",
	"v\303\245rt",
	"\303\244n",
	"\303\244r",
	"\303\245t",
	"\303\266ver",
	NULL
};

static const char **stopword_names(void)
{
	return (const char **)stopword_list_names;
}

static const uint8_t **stopwords(const char *name, int *lenptr)
{
	const struct stopword_list *ptr = stopword_lists;

	while (ptr != NULL && strcmp(ptr->name, name) != 0) {
		ptr++;
	}

	if (ptr == NULL) {
		if(lenptr) {
			*lenptr = 0;
		}
		return NULL;
	}

	if(lenptr) {
		*lenptr = ptr->length;
	}
	return (const uint8_t **)(stopword_strings + ptr->offset);
}

#endif /* STOPWORDS_H */
