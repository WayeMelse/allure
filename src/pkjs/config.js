// Textes de la page de reglages : selon la langue du telephone
// (navigator.language). Anglais par defaut.
var language = (navigator.language || 'en').substring(0, 2);

var TEXTS = {
  en: {
    intro: 'This information is used to compute heart-rate zones, calories and distance. It stays on your phone and your watch.',
    profile: 'Profile', sex: 'Sex', male: 'Male', female: 'Female',
    age: 'Age (years)', height: 'Height (cm)', weight: 'Weight (kg)',
    save: 'Save',
    stride: 'Step length', strideHint: 'Optional. When the switch is off, Allure estimates your step length from your height.', strideSwitch: 'Use my own step length', strideWalk: 'Walking step (cm)', strideRun: 'Running step (cm)'
  },
  fr: {
    intro: 'Ces informations servent \u00e0 calculer les zones cardiaques, les calories et la distance parcourue. Elles restent sur votre t\u00e9l\u00e9phone et votre montre.',
    profile: 'Profil', sex: 'Sexe', male: 'Homme', female: 'Femme',
    age: '\u00c2ge (ans)', height: 'Taille (cm)', weight: 'Poids (kg)',
    save: 'Enregistrer',
    stride: 'Longueur de pas', strideHint: 'Facultatif. Si l\u2019option est d\u00e9sactiv\u00e9e, Allure estime votre longueur de pas \u00e0 partir de votre taille.', strideSwitch: 'Utiliser ma longueur de pas', strideWalk: 'Pas en marche (cm)', strideRun: 'Pas en course (cm)'
  },
  de: {
    intro: 'Diese Angaben dienen zur Berechnung der Herzfrequenzzonen, Kalorien und Strecke. Sie bleiben auf dem Telefon und auf der Uhr.',
    profile: 'Profil', sex: 'Geschlecht', male: 'M\u00e4nnlich', female: 'Weiblich',
    age: 'Alter (Jahre)', height: 'Gr\u00f6\u00dfe (cm)', weight: 'Gewicht (kg)',
    save: 'Speichern',
    stride: 'Schrittl\u00e4nge', strideHint: 'Optional. Ist der Schalter aus, sch\u00e4tzt Allure Ihre Schrittl\u00e4nge anhand Ihrer Gr\u00f6\u00dfe.', strideSwitch: 'Eigene Schrittl\u00e4nge verwenden', strideWalk: 'Schritt beim Gehen (cm)', strideRun: 'Schritt beim Laufen (cm)'
  },
  es: {
    intro: 'Estos datos sirven para calcular las zonas de frecuencia card\u00edaca, las calor\u00edas y la distancia recorrida. Se quedan en el tel\u00e9fono y en el reloj.',
    profile: 'Perfil', sex: 'Sexo', male: 'Hombre', female: 'Mujer',
    age: 'Edad (a\u00f1os)', height: 'Altura (cm)', weight: 'Peso (kg)',
    save: 'Guardar',
    stride: 'Longitud de paso', strideHint: 'Opcional. Si el interruptor est\u00e1 desactivado, Allure estima su longitud de paso a partir de su altura.', strideSwitch: 'Usar mi longitud de paso', strideWalk: 'Paso al caminar (cm)', strideRun: 'Paso al correr (cm)'
  },
  it: {
    intro: 'Questi dati servono a calcolare le zone cardiache, le calorie e la distanza percorsa. Restano sul telefono e sull\'orologio.',
    profile: 'Profilo', sex: 'Sesso', male: 'Uomo', female: 'Donna',
    age: 'Et\u00e0 (anni)', height: 'Altezza (cm)', weight: 'Peso (kg)',
    save: 'Salva',
    stride: 'Lunghezza del passo', strideHint: 'Facoltativo. Con l\u2019interruttore spento, Allure stima la lunghezza del passo dalla tua altezza.', strideSwitch: 'Usa la mia lunghezza del passo', strideWalk: 'Passo camminando (cm)', strideRun: 'Passo correndo (cm)'
  },
  pt: {
    intro: 'Estes dados servem para calcular as zonas de frequ\u00eancia card\u00edaca, as calorias e a dist\u00e2ncia percorrida. Ficam no telefone e no rel\u00f3gio.',
    profile: 'Perfil', sex: 'Sexo', male: 'Masculino', female: 'Feminino',
    age: 'Idade (anos)', height: 'Altura (cm)', weight: 'Peso (kg)',
    save: 'Guardar',
    stride: 'Comprimento do passo', strideHint: 'Opcional. Com o interruptor desligado, o Allure estima o comprimento do passo a partir da sua altura.', strideSwitch: 'Usar o meu comprimento do passo', strideWalk: 'Passo a caminhar (cm)', strideRun: 'Passo a correr (cm)'
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
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": t.stride },
      { "type": "text", "defaultValue": t.strideHint },
      { "type": "toggle", "messageKey": "StrideCustom", "label": t.strideSwitch,
        "defaultValue": false },
      { "type": "slider", "messageKey": "StrideWalk", "label": t.strideWalk,
        "defaultValue": 70, "min": 30, "max": 100, "step": 1 },
      { "type": "slider", "messageKey": "StrideRun", "label": t.strideRun,
        "defaultValue": 100, "min": 40, "max": 160, "step": 1 }
    ]
  },
  { "type": "submit", "defaultValue": t.save }
];
