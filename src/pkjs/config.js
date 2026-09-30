// Textes de la page de reglages : selon la langue du telephone
// (navigator.language). Anglais par defaut.
var language = (navigator.language || 'en').substring(0, 2);

var TEXTS = {
  en: {
    intro: 'This information is used to compute heart-rate zones, calories and distance. It stays on your phone and your watch.',
    profile: 'Profile', sex: 'Sex', male: 'Male', female: 'Female',
    age: 'Age (years)', height: 'Height (cm)', weight: 'Weight (kg)',
    save: 'Save'
  },
  fr: {
    intro: 'Ces informations servent \u00e0 calculer les zones cardiaques, les calories et la distance parcourue. Elles restent sur votre t\u00e9l\u00e9phone et votre montre.',
    profile: 'Profil', sex: 'Sexe', male: 'Homme', female: 'Femme',
    age: '\u00c2ge (ans)', height: 'Taille (cm)', weight: 'Poids (kg)',
    save: 'Enregistrer'
  },
  de: {
    intro: 'Diese Angaben dienen zur Berechnung der Herzfrequenzzonen, Kalorien und Strecke. Sie bleiben auf dem Telefon und auf der Uhr.',
    profile: 'Profil', sex: 'Geschlecht', male: 'M\u00e4nnlich', female: 'Weiblich',
    age: 'Alter (Jahre)', height: 'Gr\u00f6\u00dfe (cm)', weight: 'Gewicht (kg)',
    save: 'Speichern'
  },
  es: {
    intro: 'Estos datos sirven para calcular las zonas de frecuencia card\u00edaca, las calor\u00edas y la distancia recorrida. Se quedan en el tel\u00e9fono y en el reloj.',
    profile: 'Perfil', sex: 'Sexo', male: 'Hombre', female: 'Mujer',
    age: 'Edad (a\u00f1os)', height: 'Altura (cm)', weight: 'Peso (kg)',
    save: 'Guardar'
  },
  it: {
    intro: 'Questi dati servono a calcolare le zone cardiache, le calorie e la distanza percorsa. Restano sul telefono e sull\'orologio.',
    profile: 'Profilo', sex: 'Sesso', male: 'Uomo', female: 'Donna',
    age: 'Et\u00e0 (anni)', height: 'Altezza (cm)', weight: 'Peso (kg)',
    save: 'Salva'
  },
  pt: {
    intro: 'Estes dados servem para calcular as zonas de frequ\u00eancia card\u00edaca, as calorias e a dist\u00e2ncia percorrida. Ficam no telefone e no rel\u00f3gio.',
    profile: 'Perfil', sex: 'Sexo', male: 'Masculino', female: 'Feminino',
    age: 'Idade (anos)', height: 'Altura (cm)', weight: 'Peso (kg)',
    save: 'Guardar'
  }
};

var t = TEXTS[language] || TEXTS.en;

module.exports = [
  { "type": "heading", "defaultValue": "Allure" },
  { "type": "text", "defaultValue": t.intro },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": t.profile },
      {
        "type": "radiogroup",
        "messageKey": "Sex",
        "label": t.sex,
        "defaultValue": "1",
        "options": [
          { "label": t.male, "value": "1" },
          { "label": t.female, "value": "0" }
        ]
      },
      { "type": "slider", "messageKey": "Age", "label": t.age,
        "defaultValue": 35, "min": 10, "max": 100, "step": 1 },
      { "type": "slider", "messageKey": "Height", "label": t.height,
        "defaultValue": 175, "min": 120, "max": 230, "step": 1 },
      { "type": "slider", "messageKey": "Weight", "label": t.weight,
        "defaultValue": 75, "min": 30, "max": 250, "step": 1 }
    ]
  },
  { "type": "submit", "defaultValue": t.save }
];
