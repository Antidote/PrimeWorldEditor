#ifndef EOBJECTTYPE_H
#define EOBJECTTYPE_H

// dunno if this is actually needed, but here it is.
enum EObjectType
{
    Actor = 0x0,
    Waypoint = 0x2,
    DoorArea = 0x3,
    Trigger = 0x4,
    Timer = 0x5,
    Counter = 0x6,
    Effect = 0x7,
    Platform = 0x8,
    Sound = 0x9,
    Generator = 0xA,
    Dock = 0xB,
    Camera = 0xC,
    CameraWaypoint = 0xD,
    NewIntroBoss = 0xE,
    SpawnPoint = 0xF,
    CameraHint = 0x10,
    Pickup = 0x11,
    MemoryRelay = 0x13,
    RandomRelay = 0x14,
    Relay = 0x15,
    Beetle = 0x16,
    HUDMemo = 0x17,
    CameraFilterKeyframe = 0x18,
    CameraBlurKeyframe = 0x19,
    DamageableTrigger = 0x1A,
    Debris = 0x1B,
    CameraShaker = 0x1C,
    ActorKeyFrame = 0x1D,
    Water = 0x20,
    Warwasp = 0x21,
    SpacePirate = 0x24,
    FlyingPirate = 0x25,
    ElitePirate = 0x26,
    MetroidBeta = 0x27,
    ChozoGhost = 0x28,
    CoverPoint = 0x2A,
    SpiderBallWaypoint = 0x2C,
    BloodFlower = 0x2D,
    FlickerBat = 0x2E,
    PathCamera = 0x2F,
    GrapplePoint = 0x30,
    PuddleSpore = 0x31,
    SpiderBallAttractionSurface = 0x33,
    PuddleToadGamma = 0x34,
    Fog = 0x35,
    FireFlea = 0x36,
    MetareeAlpha = 0x37,
    ActorRotate = 0x39,
    SpecialFunction = 0x3A,
    SpankWeed = 0x3B,
    Zoomer = 0x3D,
    PlayerHint = 0x3E,
    Ripper = 0x3F,
    PickupGenerator = 0x40,
    PointOfInterest = 0x42,
    Drone = 0x43,
    MetroidAlpha = 0x44,
    DebrisExtended = 0x45,
    Steam = 0x46,
    Ripple = 0x47,
    BallTrigger = 0x48,
    TargetingPoint = 0x49,
    ElectroMagneticPulse = 0x4A,
    IceSheegoth = 0x4B,
    PlayerActor = 0x4C,
    Flaahgra = 0x4D,
    AreaAttributes = 0x4E,
    FishCloud = 0x4F,
    FishCloudModifier = 0x50,
    VisorFlare = 0x51,
    VisorGoo = 0x53,
    JellyZap = 0x54,
    ControllerAction = 0x55,
    Switch = 0x56,
    PlayerStateChange = 0x57,
    Thardus = 0x58,
    WallCrawlerSwarm = 0x5A,
    AIJumpPoint = 0x5B,
    FlaahgraTentacle = 0x5C,
    RoomAcoustics = 0x5D,
    ColorModulate = 0x5E,
    ThardusRockProjectile = 0x5F,
    Midi = 0x60,
    StreamedAudio = 0x61,
    WorldTeleporter = 0x62,
    Repulsor = 0x63,
    GunTurret = 0x64,
    Babygoth = 0x66,
    Eyeball = 0x67,
    RadialKnockback = 0x68,
    CameraPitchVolume = 0x69,
    EnvFxDensityController = 0x6A,
    Magdolite = 0x6B,
    TeamAIMgr = 0x6C,
    SnakeWeedSwarm = 0x6D,
    ActorContraption = 0x6E,
    Oculus = 0x6F,
    Geemer = 0x70,
    SpindleCamera = 0x71,
    AtomicAlpha = 0x72,
    CameraHintTrigger = 0x73,
    RumbleEffect = 0x74,
    AmbientAI = 0x75,
    AtomicBeta = 0x77,
    Puffer = 0x79,
    Tryclops = 0x7A,
    Ridley = 0x7B,
    Seedling = 0x7C,
    ThermalHeatFader = 0x7D,
    Burrower = 0x7F,
    ScriptBeam = 0x81,
    WorldLightFader = 0x82,
    MetroidPrimeStage2 = 0x83,
    MetroidPrimeRelay = 0x84,
    MazeNode = 0x85,
    OmegaPirate = 0x86,
    PhazonPool = 0x87,
    PhazonHealingNodule = 0x88,
    NewCameraShaker = 0x89,
    ShadowProjector = 0x8A,
    BeamEnergyBall = 0x8B
};

#endif // EOBJECTTYPE_H