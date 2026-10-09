// Textes de la page de reglages : selon la langue du telephone
// (navigator.language). Anglais par defaut.
var language = (navigator.language || 'en').substring(0, 2);

var TEXTS = {
  en: {
    intro: 'This information is used to compute heart-rate zones, calories and distance. It stays on your phone and your watch.',
    profile: 'Profile', sex: 'Sex', male: 'Male', female: 'Female',
    age: 'Age (years)', height: 'Height (cm)', weight: 'Weight (kg)',
    save: 'Save',
    stride: 'Step length', strideHint: 'Optional. When the switch is off, Allure estimates your step length from your height.', strideSwitch: 'Use my own step length', strideWalk: 'Walking step (cm)', strideRun: 'Running step (cm)',
    units: 'Units', metric: 'Metric (km, kg)', imperial: 'Imperial (mi, lb)', walkSwitch: 'Use my own walking step length', runSwitch: 'Use my own running step length'
  },
  fr: {
    intro: 'Ces informations servent \u00e0 calculer les zones cardiaques, les calories et la distance parcourue. Elles restent sur votre t\u00e9l\u00e9phone et votre montre.',
    profile: 'Profil', sex: 'Sexe', male: 'Homme', female: 'Femme',
    age: '\u00c2ge (ans)', height: 'Taille (cm)', weight: 'Poids (kg)',
    save: 'Enregistrer',
    stride: 'Longueur de pas', strideHint: 'Facultatif. Si l\u2019option est d\u00e9sactiv\u00e9e, Allure estime votre longueur de pas \u00e0 partir de votre taille.', strideSwitch: 'Utiliser ma longueur de pas', strideWalk: 'Pas en marche (cm)', strideRun: 'Pas en course (cm)',
    units: 'Unit\u00e9s', metric: 'M\u00e9trique (km, kg)', imperial: 'Imp\u00e9rial (mi, lb)', walkSwitch: 'Utiliser ma longueur de pas en marche', runSwitch: 'Utiliser ma longueur de pas en course'
  },
  de: {
    intro: 'Diese Angaben dienen zur Berechnung der Herzfrequenzzonen, Kalorien und Strecke. Sie bleiben auf dem Telefon und auf der Uhr.',
    profile: 'Profil', sex: 'Geschlecht', male: 'M\u00e4nnlich', female: 'Weiblich',
    age: 'Alter (Jahre)', height: 'Gr\u00f6\u00dfe (cm)', weight: 'Gewicht (kg)',
    save: 'Speichern',
    stride: 'Schrittl\u00e4nge', strideHint: 'Optional. Ist der Schalter aus, sch\u00e4tzt Allure Ihre Schrittl\u00e4nge anhand Ihrer Gr\u00f6\u00dfe.', strideSwitch: 'Eigene Schrittl\u00e4nge verwenden', strideWalk: 'Schritt beim Gehen (cm)', strideRun: 'Schritt beim Laufen (cm)',
    units: 'Einheiten', metric: 'Metrisch (km, kg)', imperial: 'Imperial (mi, lb)', walkSwitch: 'Eigene Schrittl\u00e4nge beim Gehen verwenden', runSwitch: 'Eigene Schrittl\u00e4nge beim Laufen verwenden'
  },
  es: {
    intro: 'Estos datos sirven para calcular las zonas de frecuencia card\u00edaca, las calor\u00edas y la distancia recorrida. Se quedan en el tel\u00e9fono y en el reloj.',
    profile: 'Perfil', sex: 'Sexo', male: 'Hombre', female: 'Mujer',
    age: 'Edad (a\u00f1os)', height: 'Altura (cm)', weight: 'Peso (kg)',
    save: 'Guardar',
    stride: 'Longitud de paso', strideHint: 'Opcional. Si el interruptor est\u00e1 desactivado, Allure estima su longitud de paso a partir de su altura.', strideSwitch: 'Usar mi longitud de paso', strideWalk: 'Paso al caminar (cm)', strideRun: 'Paso al correr (cm)',
    units: 'Unidades', metric: 'M\u00e9trico (km, kg)', imperial: 'Imperial (mi, lb)', walkSwitch: 'Usar mi longitud de paso al caminar', runSwitch: 'Usar mi longitud de paso al correr'
  },
  it: {
    intro: 'Questi dati servono a calcolare le zone cardiache, le calorie e la distanza percorsa. Restano sul telefono e sull\'orologio.',
    profile: 'Profilo', sex: 'Sesso', male: 'Uomo', female: 'Donna',
    age: 'Et\u00e0 (anni)', height: 'Altezza (cm)', weight: 'Peso (kg)',
    save: 'Salva',
    stride: 'Lunghezza del passo', strideHint: 'Facoltativo. Con l\u2019interruttore spento, Allure stima la lunghezza del passo dalla tua altezza.', strideSwitch: 'Usa la mia lunghezza del passo', strideWalk: 'Passo camminando (cm)', strideRun: 'Passo correndo (cm)',
    units: 'Unit\u00e0', metric: 'Metrico (km, kg)', imperial: 'Imperiale (mi, lb)', walkSwitch: 'Usa la mia lunghezza del passo camminando', runSwitch: 'Usa la mia lunghezza del passo correndo'
  },
  pt: {
    intro: 'Estes dados servem para calcular as zonas de frequ\u00eancia card\u00edaca, as calorias e a dist\u00e2ncia percorrida. Ficam no telefone e no rel\u00f3gio.',
    profile: 'Perfil', sex: 'Sexo', male: 'Masculino', female: 'Feminino',
    age: 'Idade (anos)', height: 'Altura (cm)', weight: 'Peso (kg)',
    save: 'Guardar',
    stride: 'Comprimento do passo', strideHint: 'Opcional. Com o interruptor desligado, o Allure estima o comprimento do passo a partir da sua altura.', strideSwitch: 'Usar o meu comprimento do passo', strideWalk: 'Passo a caminhar (cm)', strideRun: 'Passo a correr (cm)',
    units: 'Unidades', metric: 'M\u00e9trico (km, kg)', imperial: 'Imperial (mi, lb)', walkSwitch: 'Usar o meu comprimento do passo a caminhar', runSwitch: 'Usar o meu comprimento do passo a correr'
  }
};

var t = TEXTS[language] || TEXTS.en;

// Libelles imperiaux deduits des libelles metriques deja traduits.
var heightFt = t.height.replace('(cm)', '(ft)');
var heightIn = t.height.replace('(cm)', '(in)');
var weightLb = t.weight.replace('(kg)', '(lb)');
var walkInch = t.strideWalk.replace('(cm)', '(in)');
var runInch = t.strideRun.replace('(cm)', '(in)');

module.exports = [
  { "type": "heading", "defaultValue": "Allure" },
  { "type": "text", "defaultValue": t.intro },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": t.units },
      {
        "type": "radiogroup",
        "messageKey": "Units",
        "label": t.units,
        "defaultValue": "0",
        "options": [
          { "label": t.metric, "value": "0" },
          { "label": t.imperial, "value": "1" }
        ]
      }
    ]
  },
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
      { "type": "slider", "messageKey": "HeightFt", "label": heightFt,
        "defaultValue": 5, "min": 4, "max": 7, "step": 1 },
      { "type": "slider", "messageKey": "HeightIn", "label": heightIn,
        "defaultValue": 9, "min": 0, "max": 11, "step": 1 },
      { "type": "slider", "messageKey": "Weight", "label": t.weight,
        "defaultValue": 75, "min": 30, "max": 250, "step": 1 },
      { "type": "slider", "messageKey": "WeightLb", "label": weightLb,
        "defaultValue": 165, "min": 66, "max": 550, "step": 1 }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": t.stride },
      { "type": "text", "defaultValue": t.strideHint },
      { "type": "toggle", "messageKey": "StrideWalkOn", "label": t.walkSwitch,
        "defaultValue": false },
      { "type": "slider", "messageKey": "StrideWalk", "label": t.strideWalk,
        "defaultValue": 70, "min": 30, "max": 100, "step": 1 },
      { "type": "slider", "messageKey": "StrideWalkInch", "label": walkInch,
        "defaultValue": 28, "min": 12, "max": 39, "step": 1 },
      { "type": "toggle", "messageKey": "StrideRunOn", "label": t.runSwitch,
        "defaultValue": false },
      { "type": "slider", "messageKey": "StrideRun", "label": t.strideRun,
        "defaultValue": 100, "min": 40, "max": 160, "step": 1 },
      { "type": "slider", "messageKey": "StrideRunInch", "label": runInch,
        "defaultValue": 39, "min": 16, "max": 63, "step": 1 }
    ]
  },
  { "type": "submit", "defaultValue": t.save }
];
