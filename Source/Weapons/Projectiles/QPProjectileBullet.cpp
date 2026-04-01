#include "QPProjectileBullet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
AQPProjectileBullet::AQPProjectileBullet()
{
	PrimaryActorTick.bCanEverTick = true;
	BulletCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BulletCollision"));
	SetRootComponent(BulletCollision);
	BulletCollision->InitSphereRadius(5.f);
	BulletCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BulletCollision->SetCollisionResponseToAllChannels(ECR_Block);
	BulletCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 1.0f;

	BulletMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BulletMesh"));
	BulletMesh->SetupAttachment(RootComponent);
	BulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InitialLifeSpan = 3.0f;

	bReplicates = true;
	SetReplicateMovement(true);
}

void AQPProjectileBullet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQPProjectileBullet, ReplicatedInitialVelocity);
}

void AQPProjectileBullet::SetBulletVelocity(const FVector& Direction, float Speed)
{
	if (ProjectileMovement) {
		const FVector CommonVelocity = Direction.GetSafeNormal() * Speed;
		ProjectileMovement->Velocity = CommonVelocity;
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;

		if (HasAuthority())
		{
			ReplicatedInitialVelocity = CommonVelocity;
		}
	}
}

void AQPProjectileBullet::OnRep_InitialVelocity()
{
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = ReplicatedInitialVelocity;
		ProjectileMovement->InitialSpeed = ReplicatedInitialVelocity.Size();
		ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
	}
}

void AQPProjectileBullet::BeginPlay()
{
	Super::BeginPlay();
	BulletCollision->OnComponentHit.AddDynamic(this, &AQPProjectileBullet::OnHit);
	PrevLocation = GetActorLocation(); //이전 위치 초기화
	if (AActor* OwnerActor = GetOwner())
	{
		//총알이 소유자(몸체 캡슐 등 전체)와 완벽하게 물리적으로 겹치지 않도록 양방향 무시 처리
		BulletCollision->IgnoreActorWhenMoving(OwnerActor, true); 
		TArray<UPrimitiveComponent*> OwnerComps;
		OwnerActor->GetComponents(OwnerComps);
		for (UPrimitiveComponent* Comp : OwnerComps)
		{
			BulletCollision->IgnoreComponentWhenMoving(Comp, true);
			Comp->IgnoreComponentWhenMoving(BulletCollision, true); // 양방향 통과 확정
		}
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		BulletCollision->IgnoreActorWhenMoving(InstigatorPawn, true); 
	}

	// 발사될 때 전 화면에 있는 다른 총알들을 찾아내 서로 양방향(Bidirectional) 물리 충돌을 무시하도록 설정
	TArray<AActor*> FoundBullets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AQPProjectileBullet::StaticClass(), FoundBullets);
	for (AActor* BulletActor : FoundBullets)
	{
		if (BulletActor != this)
		{
			BulletCollision->IgnoreActorWhenMoving(BulletActor, true);
			
			// 상대방 총알의 물리 엔진에게도 '나(방금 태어난 총알)'를 무시하라고 즉시 알려줍니다. (샷건 총알들이 공중에서 서로 부딪혀 정지하는 현상 원천 봉쇄)
			if (UPrimitiveComponent* OtherCollision = Cast<UPrimitiveComponent>(BulletActor->GetRootComponent()))
			{
				OtherCollision->IgnoreActorWhenMoving(this, true);
			}
		}
	}

	if (TrailFX) {
		TrailFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailFX, RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true, ENCPoolMethod::AutoRelease, true);
	}
	if (TrailFXComponent) {
		TrailFXComponent->SetWorldScale3D(TrailFXScale);
	}
}

void AQPProjectileBullet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AQPProjectileBullet::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 자신이거나, 주인이거나, 겹친게 총알이면 타격 판정 및 파괴를 건너뜀 (다만 물리적으로는 이미 무시 설정되었으므로 이 코드는 2차 방어막임)
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner() || OtherActor->IsA(AQPProjectileBullet::StaticClass()))
	{
		return; 
	}

	const FVector Dir = GetVelocity().GetSafeNormal();
	UGameplayStatics::ApplyPointDamage(OtherActor, Damage, Dir, Hit, GetInstigatorController(), this, DamageTypeClass);
	
	Destroy(); // 유효한 적이나 지형에 부딪히면 확실하게 파괴
}

void AQPProjectileBullet::Destroyed()
{
	if (TrailFXComponent)
	{
		// 총알이 파괴될 때 남아있는 꼬리(Trail) 이펙트도 즉시 강제로 지워줍니다.
		TrailFXComponent->DestroyComponent(); 
	}

	Super::Destroyed();
}


