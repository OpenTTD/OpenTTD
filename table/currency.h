// exchange rate    prefix
// |  separator        |     postfix
// |   |    Euro year  |       |
// |   |    |          |       |
CurrencySpec _currency_specs[] = {
{ 1,   ',', CF_NOEURO, "\xA3",   "" }, // british pounds
{ 2,   ',', CF_NOEURO, "$",      "" }, // us dollars
{ 2,   ',', CF_ISEURO, "¤",      "" }, // Euro
{ 200, ',', CF_NOEURO, "\xA5",   "" }, // yen
{ 57,  ',', 2002,     "BEF ",    "" }, // belgian franc
{ 2,   ',', CF_NOEURO,"CHF ",    "" }, // swiss franc
{ 480, ',', 2002,         "", "Dr." }, // greek drachma
{ 4,   '.', 2002,      "DM ",    "" }, // deutsche mark
{ 10,  '.', 2002,      "FF ",    "" }, // french francs
{ 376, ',', 2002,         "", " Ft" }, // forint
{ 50,  ',', CF_NOEURO,    "", " Kc" }, // czech koruna // TODO: Should use the "c" with an upside down "^"
{ 13,  '.', CF_NOEURO,    "", " Kr" }, // swedish krona
{ 130, '.', CF_NOEURO,    "", " Kr" }, // icelandic krona
{ 11,  '.', CF_NOEURO,    "", " Kr" }, // norwegian krone
{ 10,  '.', CF_NOEURO,    "", " kr" }, // danish krone
{ 2730,',', 2002,         "", " L." }, // italian lira
{ 6,   '.', CF_NOEURO,    ""," Lei" }, // romanian Lei
{ 8,   ',', 2002,         "", " MK" }, // finnish markka
{ 3,   ',', 2002,     "NLG ",    "" }, // dutch gulden
{ 5,   ' ', CF_NOEURO,    "",  " p" }, // russian rouble
{ 200, '.', 2002,     "Pts ",    "" }, // spanish pesetas
{ 19,  ',', 2002,         "", " S." }, // austrian schilling
{ 6,   ' ', CF_NOEURO,    "", " zl" }, // polish zloty
{ 1,   ' ', CF_NOEURO,    "",    "" }, // custom currency
};
