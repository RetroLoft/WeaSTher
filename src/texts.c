/*
 * User interface texts. Non-ASCII characters are in the Atari character
 * set: \x81 = u umlaut, \x82 = e acute, \x84 = a umlaut, \x94 = o umlaut.
 * The weather descriptions themselves come translated from wttr.in.
 */
#include <string.h>
#include "texts.h"

static const TEXTS texts[] = {
    {
        "nl",
        "Vandaag", "Morgen",
        { "Zondag", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag" },
        "Gevoel", "Wind", "Vocht", "Neerslag", "Zon",
        "Ververs", "Opties",
        "Ophalen...", "Bijgewerkt", "Mislukt",
        { "", "STinG niet gevonden", "Host onbekend (DNS)", "Geen verbinding",
          "Time-out", "Antwoord te groot", "HTTP-fout", "Geen geheugen" },
        "Plaats niet gevonden", "Onbekend antwoord",
        "WeaSTher instellingen",
        "Plaats:", "(leeg = automatisch)",
        "Taal:", "(nl en de fr es it ...)",
        "Eenheden:", "Metrisch", "Imperiaal",
        "Verversen:", "min. (0 = uit)", "OK", "Annuleer",
        "NOZW",
    },
    {
        "en",
        "Today", "Tomorrow",
        { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" },
        "Feels", "Wind", "Humidity", "Precip.", "Sun",
        "Refresh", "Options",
        "Fetching...", "Updated", "Failed",
        { "", "STinG not found", "Unknown host (DNS)", "No connection",
          "Timed out", "Answer too big", "HTTP error", "Out of memory" },
        "Location not found", "Unexpected answer",
        "WeaSTher settings",
        "Location:", "(empty = automatic)",
        "Language:", "(en nl de fr es it ...)",
        "Units:", "Metric", "Imperial",
        "Refresh:", "min. (0 = off)", "OK", "Cancel",
        "NESW",
    },
    {
        "de",
        "Heute", "Morgen",
        { "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag" },
        "Gef\x81hlt", "Wind", "Feuchte", "Niederschl.", "Sonne",
        "Neu", "Optionen",
        "Lade...", "Aktualisiert", "Fehler",
        { "", "STinG nicht gefunden", "Host unbekannt (DNS)", "Keine Verbindung",
          "Zeit\x81" "berschreitung", "Antwort zu gro\x9e", "HTTP-Fehler", "Kein Speicher" },
        "Ort nicht gefunden", "Unerwartete Antwort",
        "WeaSTher Einstellungen",
        "Ort:", "(leer = automatisch)",
        "Sprache:", "(de en nl fr es it ...)",
        "Einheiten:", "Metrisch", "Imperial",
        "Aktualis.:", "Min. (0 = aus)", "OK", "Abbruch",
        "NOSW",
    },
    {
        "fr",
        "Aujourd'hui", "Demain",
        { "Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi" },
        "Ressenti", "Vent", "Humidit\x82", "Pr\x82" "cip.", "Soleil",
        "Actualiser", "Options",
        "Chargement...", "Mis \x85 jour", "\x90" "chec",
        { "", "STinG introuvable", "H\x93te inconnu (DNS)", "Pas de connexion",
          "D\x82lai d\x82pass\x82", "R\x82ponse trop grande", "Erreur HTTP", "M\x82moire insuffisante" },
        "Lieu introuvable", "R\x82ponse inattendue",
        "R\x82glages WeaSTher",
        "Lieu:", "(vide = automatique)",
        "Langue:", "(fr en nl de es it ...)",
        "Unit\x82s:", "M\x82trique", "Imp\x82rial",
        "Actualiser:", "min. (0 = non)", "OK", "Annuler",
        "NESO",
    },
};

const TEXTS *texts_for(const char *lang)
{
    unsigned i;
    for (i = 0; i < sizeof texts / sizeof texts[0]; i++)
        if (!strncmp(lang, texts[i].code, 2)) return &texts[i];
    return &texts[1];
}
